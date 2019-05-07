//#define SHOW_ENABLE
#include "golos.publication_rewards_types.hpp"
#include "golos_tester.hpp"
#include "golos.posting_test_api.hpp"
#include "golos.vesting_test_api.hpp"
#include "cyber.token_test_api.hpp"
#include <math.h>
#include "../golos.publication/types.h"
#include "golos.charge_test_api.hpp"
#include "../golos.publication/config.hpp"
#include "contracts.hpp"

namespace cfg = golos::config;
using namespace fixed_point_utils;
using namespace eosio::testing;

#define PRECISION 3
#define PRECISION_DIV 1000.0
#define TOKEN_NAME "GOLOS"
constexpr uint16_t MAXTOKENPROP = 5000;
constexpr auto MAX_ARG = static_cast<double>(std::numeric_limits<fixp_t>::max());

double log2(double arg) {
    return log(arg) / log(2.0);
}

namespace golos_curation {
constexpr int64_t _2s = 2 * 2000000000000;
constexpr int64_t _m = std::numeric_limits<fixp_t>::max();
std::string func_str = std::to_string(_m) + " / ((" + std::to_string(_2s) + " / max(x, 0.1)) + 1)";
auto func = [](double x){ return static_cast<double>(_m) / ((static_cast<double>(_2s) / std::max(x, 0.000000000001)) + 1.0); };
}

class reward_calcs_tester : public golos_tester {
protected:
    symbol _token_symbol;
    golos_posting_api post;
    golos_vesting_api vest;
    golos_charge_api charge;
    cyber_token_api token;

    account_name _forum_name;
    account_name _issuer;
    vector<account_name> _users;
    account_name _stranger;
    account_name _close_acc;
    statemap _req;
    statemap _res;
    state _state;

    struct errors: contract_error_messages {
        const string not_positive       = amsg("check positive failed for time penalty func");
        const string not_monotonic      = amsg("check monotonic failed for time penalty func");
        const string fp_cast_overflow   = amsg("fp_cast: overflow");
        const string limit_no_power     = amsg("not enough power");
        const string limit_no_vesting   = amsg("insufficient effective vesting amount");
        const string delete_upvoted     = amsg("Cannot delete a comment with net positive votes.");
    } err;

    void init(int64_t issuer_funds, int64_t user_vesting_funds) {

        BOOST_CHECK_EQUAL(success(), post.init_default_params());

        auto total_funds = issuer_funds + _users.size() * user_vesting_funds;
        BOOST_CHECK_EQUAL(success(), token.create(_issuer, asset(total_funds, _token_symbol), {_issuer, cfg::charge_name, _forum_name}));
        produce_blocks();

        BOOST_CHECK_EQUAL(success(), token.issue(_issuer, _issuer, asset(total_funds, _token_symbol), "HERE COULD BE YOUR ADVERTISEMENT"));
        produce_blocks();

        BOOST_CHECK_EQUAL(success(), token.open(_forum_name, _token_symbol, _forum_name));
        BOOST_CHECK_EQUAL(success(), vest.create_vesting(_issuer, _token_symbol, cfg::control_name));
        charge.link_invoice_permission(_issuer);
        produce_blocks();

        BOOST_CHECK_EQUAL(success(), vest.open(_forum_name, _token_symbol, _forum_name));
        for (auto& u : _users) {
            BOOST_CHECK_EQUAL(success(), vest.open(u, _token_symbol, u));
            produce_blocks();
            BOOST_CHECK_EQUAL(success(), add_funds_to(u, user_vesting_funds));
            produce_blocks();
            BOOST_CHECK_EQUAL(success(), buy_vesting_for(u, user_vesting_funds));
            produce_blocks();
        }

        BOOST_CHECK_EQUAL(success(), token.open(_close_acc, _token_symbol, _close_acc));
        produce_blocks();
        BOOST_CHECK_EQUAL(success(), vest.open(_close_acc, _token_symbol, _close_acc));
        produce_blocks();

        check();
        _req.clear();
        _res.clear();
    }

public:
    reward_calcs_tester()
        : golos_tester(N(reward.calcs))
        , _token_symbol(PRECISION, TOKEN_NAME)
        , post({this, _code, _token_symbol})
        , vest({this, cfg::vesting_name, _token_symbol})
        , charge({this, cfg::charge_name, _token_symbol})
        , token({this, cfg::token_name, _token_symbol})

        , _forum_name(_code)
        , _issuer(N(issuer.acc))
        , _users{
            N(alice),  N(alice1),  N(alice2), N(alice3), N(alice4), N(alice5),
            N(bob), N(bob1), N(bob2), N(bob3), N(bob4), N(bob5),
            N(why), N(has), N(my), N(imagination), N(become), N(poor)}
        , _stranger(N(dan.larimer)) 
        , _close_acc(N(close.acc)) {

        produce_blocks(2);    // why 2?
        create_accounts({_forum_name, _issuer, cfg::vesting_name, cfg::token_name, cfg::control_name, cfg::charge_name, _stranger, cfg::publish_name, _close_acc});
        create_accounts(_users);
        produce_blocks(2);

        install_contract(_forum_name, contracts::posting_wasm(), contracts::posting_abi());
        install_contract(cfg::token_name, contracts::token_wasm(), contracts::token_abi());
        install_contract(cfg::vesting_name, contracts::vesting_wasm(), contracts::vesting_abi());
        install_contract(cfg::charge_name, contracts::charge_wasm(), contracts::charge_abi());
    }

    action_result add_funds_to(account_name user, int64_t amount) {
        auto ret = token.transfer(_issuer, user, asset(amount, _token_symbol));
        if (ret == success()) {
            _state.balances[user].tokenamount += amount / PRECISION_DIV;
        }
        return ret;
    }

    double get_converted_to_vesting(double arg) {
        double tokn_total = token.get_account(cfg::vesting_name)["balance"].as<asset>().to_real();
        double vest_total = vest.get_stats()["supply"].as<asset>().to_real();
        double price = (vest_total < 1.e-20) || (tokn_total < 1.e-20) ? 1.0 : (vest_total / tokn_total);
        return arg * price;
    }

    action_result buy_vesting_for(account_name user, int64_t amount) {
        auto ret = token.transfer(user, cfg::vesting_name, asset(amount, _token_symbol));
        if (ret == success()) {
            auto am = amount / PRECISION_DIV;
            _state.balances[user].tokenamount -= am;
            _state.balances[user].vestamount += get_converted_to_vesting(am);

            BOOST_CHECK_EQUAL(success(), vest.unlock_limit(user, asset(amount*10, _token_symbol)));
        }
        return ret;
    }

    void fill_depleted_pool(int64_t amount, size_t excluded) {
        size_t choice = 0;
        auto min_ratio = std::numeric_limits<double>::max();
        for (size_t i = 0; i < _state.pools.size(); i++) {
            if (i != excluded) {
                double cur_ratio = _state.pools[i].get_ratio();
                if (cur_ratio <= min_ratio) {
                    min_ratio = cur_ratio;
                    choice = i;
                }
            }
        }
        _state.pools[choice].funds += amount / PRECISION_DIV;
    }

    action_result add_funds_to_forum(int64_t amount) {
        auto ret = token.transfer(_issuer, _forum_name, asset(amount, _token_symbol));
        if (ret == success()) {
            BOOST_REQUIRE_MESSAGE(!_state.pools.empty(), "issue: _state.pools is empty");
            fill_depleted_pool(amount, _state.pools.size());
        }
        return ret;
    }

    action_result setrules(
        const funcparams& mainfunc,
        const funcparams& curationfunc,
        const funcparams& timepenalty,
        std::function<double(double)>&& mainfunc_,
        std::function<double(double)>&& curationfunc_,
        std::function<double(double)>&& timepenalty_,
        const limitsarg& lims = {{"0"}, {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}, {0, 0}, {0, 0, 0}},
        vector<limits::func_t>&& func_restorers = {
            [](double, double, double){ return 0.0; }
        }
    ) {
        static const std::vector<std::string> act_strings{"post", "comment", "vote", "post bandwidth"};
        static const int64_t max_arg = static_cast<int64_t>(std::numeric_limits<fixp_t>::max());

        BOOST_REQUIRE_EQUAL(act_strings.size(), lims.limitedacts.size());
        for (size_t i = 0; i < act_strings.size(); i++) {
            BOOST_REQUIRE_EQUAL(success(), charge.set_restorer(_issuer, lims.limitedacts[i].chargenum,
                lims.restorers[lims.limitedacts[i].restorernum], max_arg, max_arg, max_arg));

            produce_blocks();

            BOOST_REQUIRE_EQUAL(success(), post.set_limit(
                act_strings[i],
                lims.limitedacts[i].chargenum,
                lims.limitedacts[i].chargeprice,
                lims.limitedacts[i].cutoffval,
                lims.vestingprices.size() > i ? lims.vestingprices[i] : 0,
                lims.minvestings.size() > i ? lims.minvestings[i] : 0
            ));
            produce_blocks();
        }

        auto ret =  post.set_rules(mainfunc, curationfunc, timepenalty, MAXTOKENPROP);
        if (ret == success()) {
            double unclaimed_funds = 0.0;
            for (auto& p : _state.pools) {
                if (!p.messages.size()) {
                    unclaimed_funds += p.funds;
                }
            }
            _state.pools.erase(std::remove_if(_state.pools.begin(), _state.pools.end(),
                [](const rewardpool& p){return !p.messages.size();}),
                _state.pools.end());

            auto pools = post.get_reward_pools();
            auto created = (*pools.rbegin())["created"].as<uint64_t>();
            std::vector<double> v_prices;
            for (const auto l: lims.vestingprices) {
                v_prices.emplace_back(l >= 0 ? l / PRECISION_DIV : -1);
            }
            std::vector<double> min_vests;
            for (const auto m: lims.minvestings) {
                min_vests.emplace_back(m / PRECISION_DIV);
            }
            _state.pools.emplace_back(rewardpool(created,
                rewardrules(
                    {std::move(mainfunc_), static_cast<double>(mainfunc.maxarg)},
                    {std::move(curationfunc_), static_cast<double>(curationfunc.maxarg)},
                    {std::move(timepenalty_), static_cast<double>(timepenalty.maxarg)}
                ),
                limits{
                    .restorers = std::move(func_restorers),
                    .limitedacts = lims.limitedacts,
                    .vestingprices = v_prices,
                    .minvestings = min_vests
                }));
            _state.pools.back().funds += unclaimed_funds;
        }
        return ret;
    }

    void set_token_balance_from_table(account_name user, statemap& s) {
        auto accs = token.get_accounts(user);
        BOOST_CHECK_EQUAL(accs.size() <= 1, true); //for now we are checking just one token
        if (accs.empty()) {
            s.set_balance_token(user, 0.0);
        } else {
            auto cur = *accs.begin();
            s.set_balance_token(user, static_cast<double>(cur["payments"].as<asset>().to_real()));
        }
    }

    void set_payments_balance_from_table(account_name user, statemap& s) {
        auto accs = token.get_accounts(user);
        BOOST_CHECK_EQUAL(accs.size() <= 1, true);
        if (accs.empty()) {
            s.set_balance_payments(user, 0.0);
        } else {
            auto cur = *accs.begin();
            s.set_balance_payments(user, static_cast<double>(cur["payments"].as<asset>().to_real()));
        }
    }

    void set_vesting_balance_from_table(account_name user, statemap& s) {
        auto balances = vest.get_balances(user);
        BOOST_CHECK_EQUAL(balances.size() <= 1, true); //--/---/---/---/---/---/ just one vesting
        if (balances.empty()) {
            s.set_balance_vesting(user, 0.0);
        } else {
            auto cur = *balances.begin();
            s.set_balance_vesting(user, static_cast<double>(cur["vesting"].as<asset>().to_real()));
        }
    }

    void fill_from_state(statemap& s) const {
        s.clear();
        for (auto& b : _state.balances) {
            s.set_balance(b.first, {b.second.tokenamount, b.second.vestamount});
            if (b.first == _forum_name)
                s.set_balance(b.first, {b.second.tokenamount, b.second.vestamount, b.second.paymentsamount});
            else
                s.set_balance(_forum_name, {0.0, 0.0, 0.0});
        }
        for (auto& p : _state.pools) {
            double pool_rshares_sum = 0.0;
            double pool_rsharesfn_sum = 0.0;

            for (auto& m : p.messages) {
                double netshares = 0.0;
                double voteshares = 0.0;
                double curators_fn_sum = 0.0;
                double prev_curation_fn_val = p.rules.curationfunc(voteshares);
                for (auto& v : m.votes) {
                    netshares += v.rshares();
                    voteshares += v.voteshares();
                    double curation_fn_val = p.rules.curationfunc(voteshares);
                    if(!v.revote_diff)
                        curators_fn_sum += curation_fn_val - prev_curation_fn_val;
                    prev_curation_fn_val = curation_fn_val;
                }
                netshares  = std::min(MAX_ARG, netshares);
                voteshares = std::min(MAX_ARG, voteshares);
                s.set_message(m.key, {netshares, voteshares, curators_fn_sum});
                pool_rshares_sum = netshares + pool_rshares_sum;
                pool_rsharesfn_sum += p.rules.mainfunc(netshares);
            }
            s.set_pool(p.id, {static_cast<double>(p.messages.size()), p.funds, pool_rshares_sum, pool_rsharesfn_sum});
        }
    }

    void fill_from_tables(statemap& s) {
        s.clear();
        for (auto& user : _users) {
            set_token_balance_from_table(user, s);
            set_vesting_balance_from_table(user, s);
        }
        set_token_balance_from_table(_forum_name, s);
        set_vesting_balance_from_table(_forum_name, s);
        set_payments_balance_from_table(_forum_name, s);
        //pools
        {
            auto pools = post.get_reward_pools();
            for (auto itr = pools.begin(); itr != pools.end(); ++itr) {
                auto cur = *itr;
                s.set_pool(cur["created"].as<uint64_t>(), {
                    static_cast<double>(cur["state"]["msgs"].as<uint64_t>()),
                    static_cast<double>(cur["state"]["funds"].as<eosio::chain::asset>().to_real()),
                    static_cast<double>(WP(cur["state"]["rshares"].as<wide_t>())),
                    static_cast<double>(WP(cur["state"]["rsharesfn"].as<wide_t>()))
                });
            }
        }
        //messages
        {
            for (auto& user : _users) {
                auto msgs = post.get_messages(user);
                for (auto itr = msgs.begin(); itr != msgs.end(); ++itr) {
                    auto cur = *itr;
                    s.set_message(mssgid{user, cur["permlink"].as<std::string>()}, {
                                      static_cast<double>(FP(cur["state"]["netshares"].as<base_t>())),
                                      static_cast<double>(FP(cur["state"]["voteshares"].as<base_t>())),
                                      static_cast<double>(FP(cur["state"]["sumcuratorsw"].as<base_t>()))
                                  });

                }
            }
        }
    }

    void show(bool req = true, bool res = true) {
#ifdef SHOW_ENABLE
        if (req) {
            fill_from_state(_req);
            BOOST_TEST_MESSAGE("model:\n" << _req);
        }
        if (res) {
             fill_from_tables(_res);
             BOOST_TEST_MESSAGE("contract:\n" << _res);
        }
#endif
    }
    void pay_rewards_in_state() {
        const auto now = control->head_block_time().sec_since_epoch();
        for (auto itr_p = _state.pools.begin(); itr_p != _state.pools.end(); itr_p++) {
            auto& p = *itr_p;
            for (auto itr_m = p.messages.begin(); itr_m != p.messages.end();) {
                if (now - itr_m->created >= post.window) {
                    auto m = *itr_m;
                    double pool_rsharesfn_sum = p.get_rsharesfn_sum();

                    double payout = 0;
                    if (p.messages.size() == 1) {
                        payout = p.funds;
                    } else if (pool_rsharesfn_sum > 1.e-20) {
                        payout = (p.rules.mainfunc(m.get_rshares_sum()) * p.funds) / pool_rsharesfn_sum;
                        payout *= m.reward_weight;
                    }

                    auto curation_payout = itr_m->curators_prcnt * payout;
                    double unclaimed_funds = curation_payout;
                    std::list<std::pair<account_name, double>> cur_rewards;
                    double voteshares = 0.0;
                    double curators_fn_sum = 0.0;
                    double prev_curation_fn_val = p.rules.curationfunc(voteshares);
                    for (auto& v : m.votes) {
                        voteshares += v.voteshares();
                        double curation_fn_val = p.rules.curationfunc(voteshares);
                        double curation_fn_add = v.revote_diff ? 0.0 : curation_fn_val - prev_curation_fn_val;
                        cur_rewards.emplace_back(std::make_pair(v.voter, curation_fn_add *
                            std::min(p.rules.timepenalty(v.created - m.created), 1.0)
                        ));
                        curators_fn_sum += curation_fn_add;
                        prev_curation_fn_val = curation_fn_val;
                    }
                    if (curators_fn_sum > 1.e-20) {
                        for (auto& r : cur_rewards) {
                            r.second *= (curation_payout / curators_fn_sum);
                        }
                    }
                    for (auto& r : cur_rewards) {
                        auto cur_rwrd = get_converted_to_vesting(r.second);
                        if (_state.balances[r.first].vestclosed)
                            _state.balances[_forum_name].paymentsamount += cur_rwrd;
                        else
                            _state.balances[r.first].vestamount += cur_rwrd;
                        unclaimed_funds -= r.second;
                    }

                    auto author_payout =  payout - curation_payout;
                    double ben_payout_sum = 0.0;
                    for (auto& ben : m.beneficiaries) {
                        double ben_payout = author_payout * get_prop(ben.weight);
                        auto ben_rwrd = get_converted_to_vesting(ben_payout);
                        if (_state.balances[ben.account].vestclosed)
                            _state.balances[_forum_name].paymentsamount += ben_rwrd;
                        else
                            _state.balances[ben.account].vestamount += ben_rwrd;
                        ben_payout_sum += ben_payout;
                    }
                    author_payout -= ben_payout_sum;

                    auto author_token_rwrd = author_payout * m.tokenprop;
                    if (_state.balances[m.key.author].tokenclosed)
                        _state.balances[_forum_name].paymentsamount += author_token_rwrd;
                    else
                        _state.balances[m.key.author].tokenamount += author_token_rwrd;
                    auto author_vest_rwrd = get_converted_to_vesting(author_payout * (1.0 - m.tokenprop));
                    if (_state.balances[m.key.author].vestclosed)
                        _state.balances[_forum_name].paymentsamount += author_vest_rwrd;
                    else
                        _state.balances[m.key.author].vestamount += author_vest_rwrd;

                    p.messages.erase(itr_m++);
                    p.funds -= (payout - unclaimed_funds);

                    if (p.messages.size() == 0) {
                        auto itr_p_next = itr_p;
                        if ((++itr_p_next) != _state.pools.end()) {
                           fill_depleted_pool(p.funds, std::distance(_state.pools.begin(), itr_p));
                           itr_p->id = 0;
                        }
                    }
                } else {
                    ++itr_m;
                }
            }
        }
        _state.pools.erase(std::remove_if(_state.pools.begin(), _state.pools.end(),
            [](const rewardpool& p) {
                return (p.id == 0);
            }),
            _state.pools.end());
    }

    void check(const string& s = string()) {
        pay_rewards_in_state();
        if (!s.empty())
            BOOST_TEST_MESSAGE(s);
        produce_blocks();
        fill_from_tables(_res);
        fill_from_state(_req);

        _res.remove_same(_req);
        if (!_res.empty()) {
            BOOST_TEST_MESSAGE("contract state != model state\n diff:");
            BOOST_TEST_MESSAGE("contract:");
            BOOST_TEST_MESSAGE(_res);
            BOOST_TEST_MESSAGE("model:");
            BOOST_TEST_MESSAGE(_req);
            BOOST_REQUIRE(false);
        }
    }

    action_result create_message(
        mssgid message_id,
        mssgid parent_message_id = {N(), "parentprmlnk"},
        vector<beneficiary> beneficiaries = {},
        uint16_t tokenprop = MAXTOKENPROP,
        bool vestpayment = false,
        std::string title = "headermssg",
        std::string body = "bodymssg",
        std::string language = "languagemssg",
        std::vector<std::string> tags = {"tag"},
        std::string json_metadata = "jsonmetadata",
        uint16_t curators_prcnt = 7100
    ) {
        const auto current_time = control->head_block_time().sec_since_epoch();
        auto ret = post.create_msg(message_id, parent_message_id, beneficiaries, tokenprop, vestpayment, title, body, language, tags, json_metadata, curators_prcnt);
//        message_key key{author, permlink};

        auto reward_weight = 0.0;
        string ret_str = ret;
        if ((ret == success()) || (ret_str.find("forum::apply_limits:") != string::npos)) {
            reward_weight = _state.pools.back().lims.apply(
                parent_message_id.author ? limits::COMM : limits::POST,
                _state.pools.back().charges[message_id.author],
                _state.balances[message_id.author].vestamount,
                seconds(current_time).count(),
                vestpayment);
            BOOST_REQUIRE_MESSAGE(((ret == success()) == (reward_weight >= 0.0)), "wrong ret_str: " + ret_str
                + "; vesting = " + std::to_string(_state.balances[message_id.author].vestamount)
                + "; reward_weight = " + std::to_string(reward_weight));
        }

        if (ret == success()) {
            _state.pools.back().messages.emplace_back(message(
                message_id,
                static_cast<double>(tokenprop) / static_cast<double>(cfg::_100percent),
                static_cast<double>(current_time),
                beneficiaries, reward_weight,
                static_cast<double>(curators_prcnt) / static_cast<double>(cfg::_100percent)));
        } else {
            message_id.permlink = std::string();
        }
        return ret;
    }

    action_result delete_message(mssgid message_id) {
        return post.delete_msg(message_id);
    }

    action_result addvote(account_name voter, mssgid message_id, int32_t weight) {
        auto ret = weight == 0
            ? post.unvote(voter, message_id)
            : weight > 0
                ? post.upvote(voter, message_id, weight)
                : post.downvote(voter, message_id, -weight);

//        message_key msg_key{author, permlink};

        string ret_str = ret;
        const auto current_time = control->head_block_time().sec_since_epoch();
        if ((ret == success()) || (ret_str.find("forum::apply_limits:") != string::npos)) {
            auto apply_ret = _state.pools.back().lims.apply(
                limits::VOTE, _state.pools.back().charges[voter],
                _state.balances[voter].vestamount, seconds(current_time).count(), get_prop(std::abs(weight)));
            BOOST_REQUIRE_MESSAGE(((ret == success()) == (apply_ret >= 0.0)), "wrong ret_str: " + ret_str
                + "; vesting = " + std::to_string(_state.balances[voter].vestamount)
                + "; apply_ret = " + std::to_string(apply_ret));
        }

        if (ret == success()) {
            for (auto& p : _state.pools) {
                for (auto& m : p.messages) {
                    if (m.key == message_id) {
                        auto vote_itr = std::find_if(m.votes.begin(), m.votes.end(), [voter](const vote& v) {return v.voter == voter;});
                        if (vote_itr == m.votes.end())
                            m.votes.emplace_back(vote{
                                voter,
                                std::min(get_prop(weight), 1.0),
                                PRECISION_DIV * static_cast<double>(FP(_state.balances[voter].vestamount)), //raw amount
                                static_cast<double>(current_time)
                            });
                        else {
                            vote_itr->revote_diff = std::min(get_prop(weight), 1.0) - vote_itr->weight;
                            vote_itr->revote_vesting =
                                PRECISION_DIV * static_cast<double>(FP(_state.balances[voter].vestamount));
                        }
                        return ret;
                    }
                }
            }
            BOOST_REQUIRE_MESSAGE(false, "addvote: ret == success(), but message not found in state");
        }
        return ret;
    }

    action_result close_token_acc(name owner, symbol symbol) {
        auto ret = token.close(owner, symbol);
        if (ret == success())
            _state.balances[owner].tokenclosed = true;
        return ret;
    }
    
    action_result close_vest_acc(name owner, symbol symbol) {
        auto ret = vest.close(owner, symbol);
        if (ret == success())
            _state.balances[owner].vestclosed = true;
        return ret;
    }
};

BOOST_AUTO_TEST_SUITE(reward_calcs_tests)

BOOST_FIXTURE_TEST_CASE(basic_tests, reward_calcs_tester) try {
    BOOST_TEST_MESSAGE("Basic publication_rewards tests");
    auto bignum = 500000000000;
    init(bignum, 500000);
    produce_blocks();
    BOOST_TEST_MESSAGE("--- setrules");
    BOOST_CHECK_EQUAL(err.not_monotonic,
        setrules({"x", bignum}, {"log2(x + 1.0)", bignum}, {"1-x", bignum},
        [](double x){ return x; }, [](double x){ return log2(x + 1.0); }, [](double x){ return 1.0-x; }));

    BOOST_CHECK_EQUAL(err.fp_cast_overflow,
        setrules({"x", std::numeric_limits<base_t>::max()}, {"sqrt(x)", bignum}, {"1", bignum},
        [](double x){ return x; }, [](double x){ return sqrt(x); }, [](double x){ return 1.0; }));

    BOOST_CHECK_EQUAL(err.not_positive,
        setrules({"x", bignum}, {"sqrt(x)", bignum}, {"-10.0+x", 15},
        [](double x){ return x; }, [](double x){ return sqrt(x); }, [](double x){ return -10.0 + x; }));

    BOOST_CHECK_EQUAL(success(), setrules({"x^2", static_cast<base_t>(sqrt(bignum))}, {"sqrt(x)", bignum}, {"1", bignum},
        [](double x){ return x * x; }, [](double x){ return sqrt(x); }, [](double x){ return 1.0; }));
    check();

    BOOST_TEST_MESSAGE("--- create_message: bob4");
    BOOST_CHECK_EQUAL(success(), create_message({N(bob4), "permlink"}));
    check();
    BOOST_TEST_MESSAGE("--- bob5 voted (-50%) against bob4");
    BOOST_CHECK_EQUAL(success(), addvote(N(bob5), {N(bob4), "permlink"}, -5000));
    check();
    BOOST_TEST_MESSAGE("--- bob4 voted (100%) for bob4");
    BOOST_CHECK_EQUAL(success(), addvote(N(bob4), {N(bob4), "permlink"}, 10000));
    check();
    BOOST_TEST_MESSAGE("--- bob3 voted (100%) for bob4");
    BOOST_CHECK_EQUAL(success(), addvote(N(bob3), {N(bob4), "permlink"}, 10000));
    check();
    BOOST_TEST_MESSAGE("--- bob3 revoted (-100%) against bob4");
    BOOST_CHECK_EQUAL(success(), addvote(N(bob3), {N(bob4), "permlink"}, -10000));
    check();

    BOOST_TEST_MESSAGE("--- create_message: alice");
    BOOST_CHECK_EQUAL(success(), create_message({N(alice), "permlink"}));
    check();
    BOOST_CHECK_EQUAL(success(), setrules({"x", bignum}, {"sqrt(x)", bignum}, {"1", bignum},
        [](double x){ return x; }, [](double x){ return sqrt(x); }, [](double x){ return 1.0; }));
    check();
    BOOST_TEST_MESSAGE("--- add_funds_to_forum");
    BOOST_CHECK_EQUAL(success(), add_funds_to_forum(50000));
    check();
    BOOST_TEST_MESSAGE("--- create_message: bob");
    BOOST_CHECK_EQUAL(success(), create_message({N(bob), "permlink1"}));
    check();
    BOOST_TEST_MESSAGE("--- create_message: alice (1)");
    BOOST_CHECK_EQUAL(success(), create_message({N(alice), "permlink1"}));
    check();
    BOOST_TEST_MESSAGE("--- alice1 voted for alice");
    BOOST_CHECK_EQUAL(success(), addvote(N(alice1), {N(alice), "permlink"}, 100));
    check();
    BOOST_TEST_MESSAGE("--- trying to delete alice's message");
    BOOST_CHECK_EQUAL(err.delete_upvoted, delete_message({N(alice), "permlink"}));
    BOOST_TEST_MESSAGE("--- bob1 voted (1%) for bob");
    BOOST_CHECK_EQUAL(success(), addvote(N(bob1), {N(bob), "permlink1"}, 100));
    check();
    BOOST_TEST_MESSAGE("--- bob1 revoted (-50%) against bob");
    BOOST_CHECK_EQUAL(success(), addvote(N(bob1), {N(bob), "permlink1"}, -5000));
    check();
    BOOST_TEST_MESSAGE("--- bob2 voted (100%) for bob");
    BOOST_CHECK_EQUAL(success(), addvote(N(bob2), {N(bob), "permlink1"}, 10000));
    check();
    BOOST_TEST_MESSAGE("--- alice2 voted (100%) for alice (1)");
    BOOST_CHECK_EQUAL(success(), addvote(N(alice2), {N(alice), "permlink1"}, 10000));
    check();
    BOOST_TEST_MESSAGE("--- alice3 flagged (80%) against bob");
    BOOST_CHECK_EQUAL(success(), addvote(N(alice3), {N(bob), "permlink1"}, -8000));
    check();
    BOOST_TEST_MESSAGE("--- alice4 voted (30%) for alice (1)");
    BOOST_CHECK_EQUAL(success(), addvote(N(alice4), {N(alice), "permlink1"}, 3000));
    check();
    BOOST_TEST_MESSAGE("--- alice4 revoted (100%) for alice (1)");
    BOOST_CHECK_EQUAL(success(), addvote(N(alice4), {N(alice), "permlink1"}, 10000));
    check();

    BOOST_TEST_MESSAGE("--- add_funds_to_forum");
    BOOST_CHECK_EQUAL(success(), add_funds_to_forum(100000));
    produce_blocks();     // push transactions before run() call
    check();

    BOOST_TEST_MESSAGE("--- waiting");
    produce_blocks(golos::seconds_to_blocks(99));   // TODO: remove magic number
    check();
    BOOST_TEST_MESSAGE("--- create_message: why");
    BOOST_CHECK_EQUAL(success(), create_message({N(why), "why-not"}, {N(), ""}, {{N(alice5), 5000}, {N(bob5), 2500}}));
    check();
    BOOST_TEST_MESSAGE("--- add_funds_to_forum");
    BOOST_CHECK_EQUAL(success(), add_funds_to_forum(100000));
    check();
    BOOST_TEST_MESSAGE("--- why voted (100%) for why");
    BOOST_CHECK_EQUAL(success(), addvote(N(why), {N(why), "why-not"}, 10000));
    produce_blocks();     // push transactions before run() call
    check();
    produce_blocks(golos::seconds_to_blocks(99));   // TODO: remove magic number
    BOOST_TEST_MESSAGE("--- rewards");
    check();
    show();
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(timepenalty_test, reward_calcs_tester) try {
    BOOST_TEST_MESSAGE("Simple timepenalty test");
    auto bignum = 500000000000;
    init(bignum, 500000);
    produce_blocks();

    BOOST_CHECK_EQUAL(success(), setrules({"x", bignum}, {"x", bignum}, {"x/50", 50},
        [](double x){ return x; }, [](double x){ return x; }, [](double x){ return x / 50.0; }));
    check();
    BOOST_TEST_MESSAGE("--- add_funds_to_forum");
    BOOST_CHECK_EQUAL(success(), add_funds_to_forum(50000));
    check();
    BOOST_TEST_MESSAGE("--- create_message: bob");
    BOOST_CHECK_EQUAL(success(), create_message({N(bob), "permlink"}));
    check();    // NOTE: `check()` calls `produce_blocks()` internally, it's not always expected, TODO: fix?
    BOOST_TEST_MESSAGE("--- voting");
    const auto n_voters = 6;
    const auto votes_step = 1;  // Note: `check()` adds 1 block
    for (size_t i = 0; i < n_voters; i++) {
        BOOST_CHECK_EQUAL(success(), addvote(_users[i], {N(bob), "permlink"}, 10000));
        check();
        produce_blocks(votes_step);
    }
    produce_blocks(golos::seconds_to_blocks(post.window) - 1 - n_voters * (1 + votes_step));
    BOOST_TEST_MESSAGE("--- state before post close");
    show();
    BOOST_TEST_MESSAGE("--- close post and check rewards");
    check();
    show();
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(limits_test, reward_calcs_tester) try {
    BOOST_TEST_MESSAGE("Simple limits test");
    auto bignum = 500000000000;
    init(bignum, 500000);

    name action = "calcrwrdwt"_n;
    auto auth = authority(1, {}, {
        {.permission = {golos::config::charge_name, golos::config::code_name}, .weight = 1}
    });
    set_authority(_code, action, auth, "active");
    link_authority(_code, _code, action, action);

    auto params = "[" + post.get_str_curators_prcnt(0, post.max_curators_prcnt) + "]";
    BOOST_CHECK_EQUAL(success(), post.set_params(params));

    BOOST_CHECK_EQUAL(success(), setrules({"x", bignum}, {"sqrt(x)", bignum}, {"x / 1800", 1800},
        [](double x){ return x; }, [](double x){ return sqrt(x); }, [](double x){ return x / 1800.0; },
        {
            .restorers = {"t / 250", "sqrt(v / 500000) * (t / 150)"},
            .limitedacts = {
                {.chargenum = 1, .restorernum = 1, .cutoffval = 20000, .chargeprice = 9900}, //POST
                {.chargenum = 1, .restorernum = 1, .cutoffval = 30000, .chargeprice = 1000}, //COMMENT
                {.chargenum = 0, .restorernum = 0, .cutoffval = 10000, .chargeprice = 1000}, //VOTE
                {.chargenum = 1, .restorernum = 1, .cutoffval = 10000, .chargeprice = 0}},   //POST BW
            .vestingprices = {150000, -1},
            .minvestings = {300000, 100000, 100000}
        }, {
            [](double p, double v, double t){ return t / 250.0; },
            [](double p, double v, double t){ return sqrt(v / (500000.0 / PRECISION_DIV)) * (t / 150.0); }  // !Vesting
        })
    );

    auto create_msg = [&](
            mssgid message_id,
            mssgid parent_id = {N(), "parentprmlnk"},
            bool vest_payment = false,
            uint16_t curators_prcnt = 0) {
        return create_message(
            message_id,
            parent_id,
            {},
            5000,
            vest_payment,
            "headermssg",
            "bodymssg",
            "languagemssg",
            {{"tag"}},
            "jsonmetadata",
            curators_prcnt
            );
    };

    BOOST_TEST_MESSAGE("--- add_funds_to_forum");
    BOOST_CHECK_EQUAL(success(), add_funds_to_forum(100000));
    check();

    BOOST_CHECK_EQUAL(success(), create_msg({N(bob), "permlink"}));
    BOOST_CHECK_EQUAL(success(), create_msg({N(bob), "permlink1"}));
    BOOST_CHECK_EQUAL(err.limit_no_power, create_msg({N(bob), "permlink2"}));
    BOOST_TEST_MESSAGE("--- comments");
    for (size_t i = 0; i < 10; i++) {   // TODO: remove magic number, 10 must be derived from some constant used in rules
        BOOST_CHECK_EQUAL(success(), create_msg({N(bob), "comment" + std::to_string(i)},
                                                {N(bob), "permlink"}));
    }
    BOOST_CHECK_EQUAL(err.limit_no_power, create_msg({N(bob), "oops"},
                                                     {N(bob), "permlink"}));
    BOOST_CHECK_EQUAL(success(), create_msg({N(bob), "i-can-pay-for-posting"}, {N(), ""}, true));
    BOOST_CHECK_EQUAL(err.limit_no_power,
        create_msg({N(bob), "only-if-it-is-not-a-comment"}, {N(bob), "permlink"}, true));
    BOOST_CHECK_EQUAL(success(), addvote(N(alice), {N(bob), "i-can-pay-for-posting"}, 10000));
    BOOST_CHECK_EQUAL(success(), addvote(N(bob), {N(bob), "i-can-pay-for-posting"}, 10000)); //He can also vote
    BOOST_CHECK_EQUAL(success(), create_msg({N(bob1), "permlink"}));
    produce_blocks();     // push transactions before run() call

    BOOST_TEST_MESSAGE("--- waiting");
    produce_blocks(golos::seconds_to_blocks(150));  // TODO: remove magic number
    check();
    produce_blocks(golos::seconds_to_blocks(150));  // TODO: remove magic number
    check();
    BOOST_CHECK_EQUAL(err.limit_no_power, create_msg({N(bob), "limit-no-power"}));
    produce_blocks(golos::seconds_to_blocks(45));   // TODO: remove magic number
    BOOST_CHECK_EQUAL(success(), create_msg({N(bob), "test"}));
    check();
    BOOST_CHECK_EQUAL(success(), setrules({"x", bignum}, {"sqrt(x)", bignum}, {"x / 1800", 1800},
        [](double x){ return x; }, [](double x){ return sqrt(x); }, [](double x){ return x / 1800.0; },
        {
            .restorers = {"t"},
            .limitedacts = {
                {.chargenum = 0, .restorernum = 0, .cutoffval = 0, .chargeprice = 0}, //POST
                {.chargenum = 0, .restorernum = 0, .cutoffval = 0, .chargeprice = 0}, //COMMENT
                {.chargenum = 0, .restorernum = 0, .cutoffval = 0, .chargeprice = 0}, //VOTE
                {.chargenum = 0, .restorernum = 0, .cutoffval = 0, .chargeprice = 0}},   //POST BW
            .vestingprices = {-1, -1},
            .minvestings = {400000, 0, 0}
        }, {
            [](double p, double v, double t){ return 0.0; },
            [](double p, double v, double t){ return 0.0; }
        })
    );
    check();
    BOOST_CHECK_EQUAL(err.limit_no_vesting, create_msg({N(bob), "limit-no-vesting"}));
    BOOST_CHECK_EQUAL(success(), create_msg({N(bob1), "test2"}));
    check();
    show();
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(rshares_sum_overflow_test, reward_calcs_tester) try {
    BOOST_TEST_MESSAGE("rshares_sum_overflow_test");
    auto bignum = 500000000000;
    auto fixp_max = std::numeric_limits<fixp_t>::max();
    init(bignum, fixp_max / 2);
    produce_blocks();

    BOOST_TEST_MESSAGE("--- setrules");
    BOOST_CHECK_EQUAL(success(), setrules({"x", fixp_max}, {"sqrt(x)", fixp_max}, {"1", fixp_max},
        [](double x){ return x; }, [](double x){ return sqrt(x); }, [](double x){ return 1.0; }));
    check();

    BOOST_TEST_MESSAGE("--- add_funds_to_forum");
    BOOST_CHECK_EQUAL(success(), add_funds_to_forum(50000));
    check();

    for (size_t i = 0; i < 10; i++) {   // TODO: remove magic number, 10 must be derived from some constant used in rules
        BOOST_TEST_MESSAGE("--- create_message: " << name{_users[i]}.to_string());
        BOOST_CHECK_EQUAL(success(), create_message({_users[i], "permlink"}));
        check();
        BOOST_TEST_MESSAGE("--- " << name{_users[i]}.to_string() << " voted for self");
        BOOST_CHECK_EQUAL(success(), addvote(_users[i], {_users[i], "permlink"}, 10000));
        check();
    }
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(message_netshares_overflow_test, reward_calcs_tester) try {
    BOOST_TEST_MESSAGE("rshares_sum_overflow_test");
    auto bignum = 500000000000;
    auto fixp_max = std::numeric_limits<fixp_t>::max();
    init(bignum, fixp_max / 5);
    produce_blocks();

    BOOST_TEST_MESSAGE("--- setrules");
    BOOST_CHECK_EQUAL(success(), setrules({"x", fixp_max}, {"sqrt(x)", fixp_max}, {"1", fixp_max},
        [](double x){ return x; }, [](double x){ return sqrt(x); }, [](double x){ return 1.0; }));
    check();

    BOOST_TEST_MESSAGE("--- add_funds_to_forum");
    BOOST_CHECK_EQUAL(success(), add_funds_to_forum(50000));
    check();

    BOOST_TEST_MESSAGE("--- create_message: alice");
    mssgid message_id{N(alice), "permlink"};
    BOOST_CHECK_EQUAL(success(), create_message(message_id));
    check();
    auto pool_id = post.get_reward_pools().rbegin()->operator[]("created").as<uint64_t>();

    for (size_t i = 0; i < 10; i++) {
        BOOST_TEST_MESSAGE("--- " << name{_users[i]}.to_string() << " voted for alice");
        BOOST_CHECK_EQUAL(success(), addvote(_users[i], message_id, 2000));
        produce_block();
        fill_from_tables(_res);
        BOOST_CHECK_EQUAL(_res[statemap::get_message_str(message_id) + "netshares"].val, _res.get_pool(pool_id).rshares);
        BOOST_CHECK_GT(_res[statemap::get_message_str(message_id) + "netshares"].val, 0);
        BOOST_CHECK_GT( _res.get_pool(pool_id).rsharesfn, 0);
    }
    for (size_t i = 0; i < 10; i++) {
        BOOST_TEST_MESSAGE("--- " << name{_users[i]}.to_string() << " revoted for alice");
        BOOST_CHECK_EQUAL(success(), addvote(_users[i], message_id, 4000));
        produce_block();
        fill_from_tables(_res);
        BOOST_CHECK_EQUAL(_res[statemap::get_message_str(message_id) + "netshares"].val, _res.get_pool(pool_id).rshares);
        BOOST_CHECK_GT(_res[statemap::get_message_str(message_id) + "netshares"].val, 0);
        BOOST_CHECK_GT( _res.get_pool(pool_id).rsharesfn, 0);

    }
    for (size_t i = 10; i < 15; i++) {
        BOOST_TEST_MESSAGE("--- " << name{_users[i]}.to_string() << " voted against alice");
        BOOST_CHECK_EQUAL(success(), addvote(_users[i], message_id, -10000));
        produce_block();
        fill_from_tables(_res);
        BOOST_CHECK_EQUAL(_res[statemap::get_message_str(message_id) + "netshares"].val, _res.get_pool(pool_id).rshares);
        BOOST_CHECK_GE( _res.get_pool(pool_id).rsharesfn, 0);
    }

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(golos_curation_test, reward_calcs_tester) try {
    BOOST_TEST_MESSAGE("golos_curation_test");
    int64_t maxfp = std::numeric_limits<fixp_t>::max();
    auto bignum = 500000000000;
    init(bignum, 500000);
    produce_blocks();
    BOOST_TEST_MESSAGE("--- setrules");
    using namespace golos_curation;
    BOOST_CHECK_EQUAL(success(), setrules({"x", maxfp}, {golos_curation::func_str, maxfp}, {"1", bignum},
        [](double x){ return x; }, golos_curation::func, [](double x){ return 1.0; }));
    check();

    BOOST_TEST_MESSAGE("--- add_funds_to_forum");
    BOOST_CHECK_EQUAL(success(), add_funds_to_forum(50000));
    check();

    BOOST_TEST_MESSAGE("--- create_message: " << name{_users[0]}.to_string());
    BOOST_CHECK_EQUAL(success(), create_message({_users[0], "permlink"}));
    check();

    for (size_t i = 0; i < 10; i++) {
        BOOST_TEST_MESSAGE("--- " << name{_users[i]}.to_string() << " voted");
        BOOST_CHECK_EQUAL(success(), addvote(_users[i], {_users[0], "permlink"}, 10000));
        check();
    }
    produce_blocks(golos::seconds_to_blocks(150));
    check();
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(golos_close_acc_test, reward_calcs_tester) try {
    BOOST_TEST_MESSAGE("golos_close_acc_test");
    int64_t maxfp = std::numeric_limits<fixp_t>::max();
    auto bignum = 500000000000;
    init(bignum, 500000);
    produce_blocks();
    BOOST_TEST_MESSAGE("--- setrules");
    using namespace golos_curation;
    BOOST_CHECK_EQUAL(success(), setrules({"x", maxfp}, {golos_curation::func_str, maxfp}, {"1", bignum},
        [](double x){ return x; }, golos_curation::func, [](double x){ return 1.0; }));
    check();

    BOOST_TEST_MESSAGE("--- add_funds_to_forum");
    BOOST_CHECK_EQUAL(success(), add_funds_to_forum(50000));
    check();

    BOOST_TEST_MESSAGE("--- checking payout if account is closed");
    BOOST_TEST_MESSAGE("--- create_message: " << name{_close_acc}.to_string());
    BOOST_CHECK_EQUAL(success(), create_message({_close_acc, "permlink"}));
    check();

    BOOST_TEST_MESSAGE("--- " << name{_users[0]}.to_string() << " voted");
    BOOST_CHECK_EQUAL(success(), addvote(_users[0], {_close_acc, "permlink"}, 10000));
    check();

    BOOST_CHECK_EQUAL(success(), close_token_acc(_close_acc, _token_symbol));
    BOOST_CHECK_EQUAL(success(), close_vest_acc(_close_acc, _token_symbol));

    produce_blocks(golos::seconds_to_blocks(post.window));
    check();
} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
