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
        BOOST_CHECK_EQUAL(success(), token.create(_issuer, asset(total_funds, _token_symbol)));
        produce_blocks();

        BOOST_CHECK_EQUAL(success(), token.issue(_issuer, _issuer, asset(total_funds, _token_symbol), "HERE COULD BE YOUR ADVERTISEMENT"));
        produce_blocks();

        BOOST_CHECK_EQUAL(success(), token.open(_forum_name, _token_symbol, _forum_name));
        BOOST_CHECK_EQUAL(success(), vest.create_vesting(_issuer, _token_symbol, cfg::control_name));
        produce_blocks();

        BOOST_CHECK_EQUAL(success(), vest.open(_forum_name, _token_symbol, _forum_name));
        add_vesting(_users, user_vesting_funds);

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
            N(why), N(has), N(my), N(imagination), N(become), N(poor),
            N(voter),  N(voter1),  N(voter2),
            N(voter3), N(voter4), N(voter5), N(votersix)}
        , _stranger(N(dan.larimer)) {

        produce_blocks(2);    // why 2?
        create_accounts({_forum_name, _issuer, cfg::vesting_name, cfg::token_name, cfg::control_name, cfg::charge_name, _stranger, cfg::publish_name});
        create_accounts(_users);
        produce_blocks(2);
        install_contract(config::msig_account_name, contracts::msig_wasm(), contracts::msig_abi());
        install_contract(cfg::token_name, contracts::token_wasm(), contracts::token_abi());
        vest.add_changevest_auth_to_issuer(_issuer, cfg::control_name);
        vest.initialize_contract(cfg::token_name);
        charge.initialize_contract();
        post.initialize_contract(cfg::token_name, cfg::charge_name);

        set_authority(_issuer, cfg::invoice_name, create_code_authority({charge._code, post._code}), "active");
        link_authority(_issuer, charge._code, cfg::invoice_name, N(use));
        link_authority(_issuer, charge._code, cfg::invoice_name, N(usenotifygt));
        link_authority(_issuer, vest._code, cfg::invoice_name, N(retire));
    }

    void add_vesting(std::vector<account_name> users, int64_t funds) {
        for (auto& u : users) {
            if (vest.get_balance_raw(u).is_null()) {
                BOOST_CHECK_EQUAL(success(), vest.open(u, _token_symbol, u));
                produce_blocks();
            }
            BOOST_CHECK_EQUAL(success(), add_funds_to(u, funds));
            produce_blocks();
            BOOST_CHECK_EQUAL(success(), buy_vesting_for(u, funds));
            produce_blocks();
        }
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
        asset quantity = asset(amount, _token_symbol);
        auto ret = token.transfer(_issuer, _forum_name, quantity);
        if (ret == success()) {
            BOOST_REQUIRE_MESSAGE(!_state.pools.empty(), "issue: _state.pools is empty");
            fill_depleted_pool(amount, _state.pools.size());
            _state.balances[_forum_name].tokenamount += quantity.to_real();
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
        limitsarg lims = {{"0"}, {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}, {0, 0}, {0, 0, 0}},
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
            s.set_balance_token(user, static_cast<double>(cur["balance"].as<asset>().to_real()));
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

    void fill_from_state(statemap& s) {
        s.clear();
        for (auto& b : _state.balances) {
            s.set_balance(b.first, {b.second.tokenamount, b.second.vestamount, b.second.paymentsamount});
        }
        if (_state.balances.find(_forum_name) == _state.balances.end())
            s.set_balance(_forum_name, {0.0, 0.0, 0.0});
        else
            s.set_balance(_forum_name, {_state.balances[_forum_name].tokenamount, _state.balances[_forum_name].vestamount, _state.balances[_forum_name].paymentsamount});
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
            set_payments_balance_from_table(user, s);
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

    void pay(
            account_name from,
            account_name to, 
            double payout,
            payment_t mode) 
    {
        if (mode == payment_t::TOKEN) {
            if (!_state.balances[to].tokenclosed)
                _state.balances[to].paymentsamount += payout;
            else
                _state.balances[from].paymentsamount += payout;
        } else if (mode == payment_t::VESTING) {
            if (!_state.balances[to].vestclosed)
                _state.balances[to].vestamount += get_converted_to_vesting(payout);
            else
                _state.balances[from].paymentsamount += payout;
        }
        _state.balances[from].tokenamount -= payout;
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
                        payout = std::min(payout, m.max_payout);
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
                        auto dlg_payout_sum = 0.0;
                        for (auto& dlg : _state.delegators) {
                            if (r.first == dlg.delegatee) {
                                auto available_vesting = _state.balances[r.first].vestamount - _state.dlg_balances[r.first].delegated;
                                auto effective_vesting = available_vesting + _state.dlg_balances[r.first].received;
                                auto interest_rate = static_cast<uint16_t>(dlg.amount * dlg.interest_rate / effective_vesting);
                                BOOST_CHECK(interest_rate <= cfg::_100percent);
                                if (interest_rate == 0)
                                    continue;
                                auto dlg_payout = r.second * interest_rate / golos::config::_100percent;
                                _state.balances[dlg.delegator].vestamount += get_converted_to_vesting(dlg_payout);
                                _state.balances[_forum_name].tokenamount -= dlg_payout;

                                dlg_payout_sum += dlg_payout;
                            }
                        }
                        pay(_forum_name, r.first, r.second - dlg_payout_sum, payment_t::VESTING);
                        unclaimed_funds -= r.second;
                    }

                    auto author_payout =  payout - curation_payout;
                    double ben_payout_sum = 0.0;
                    for (auto& ben : m.beneficiaries) {
                        double ben_payout = author_payout * get_prop(ben.weight);
                        pay(_forum_name, ben.account, ben_payout, payment_t::VESTING);
                        ben_payout_sum += ben_payout;
                    }
                    author_payout -= ben_payout_sum;

                    auto author_token_rwrd = author_payout * m.tokenprop;
                    pay(_forum_name, m.key.author, author_token_rwrd, payment_t::TOKEN); 
                    pay(_forum_name, m.key.author, author_payout - author_token_rwrd, payment_t::VESTING);

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
        uint16_t curators_prcnt = 7100,
        optional<asset> max_payout = optional<asset>()
    ) {
        const auto current_time = control->head_block_time().sec_since_epoch();
        auto ret = post.create_msg(message_id, parent_message_id, beneficiaries, tokenprop, vestpayment, title, body, language, tags, json_metadata, curators_prcnt, max_payout);
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
            auto msg_max_payout = static_cast<double>(MAX_ASSET_AMOUNT) / PRECISION_DIV;
            if (!!max_payout) {
                msg_max_payout = static_cast<double>(max_payout->get_amount()) / PRECISION_DIV;
            }
            _state.pools.back().messages.emplace_back(message(
                message_id,
                static_cast<double>(tokenprop) / static_cast<double>(cfg::_100percent),
                static_cast<double>(current_time),
                beneficiaries, reward_weight,
                static_cast<double>(curators_prcnt) / static_cast<double>(cfg::_100percent), msg_max_payout));
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
                                PRECISION_DIV * static_cast<double>(FP(_state.balances[voter].vestamount + _state.dlg_balances[voter].received)), //raw amount
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

    action_result retire_vest(asset quantity, name user, name issuer) {
        auto ret = vest.retire(quantity, user, issuer);
        if (ret == success())
            _state.balances[user].vestamount = 0.0;
        return ret;
    }

    action_result delegate_vest(account_name from, account_name to, asset quantity, uint16_t interest_rate) {
        auto ret = vest.delegate(from, to, quantity, interest_rate);
        if (ret == success()) {
            _state.delegators.emplace_back(delegation{
                from,
                to,
                interest_rate,
                quantity.to_real()
            });
            _state.dlg_balances[from].delegated += quantity.to_real(); 
            _state.dlg_balances[to].received += quantity.to_real();
        }
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
    BOOST_CHECK_EQUAL(success(), post.closemssgs());
    check();
    show();
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(limits_test, reward_calcs_tester) try {
    BOOST_TEST_MESSAGE("Simple limits test");
    auto bignum = 500000000000;
    init(bignum, 500000);

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
    BOOST_CHECK_EQUAL(success(), post.closemssgs());
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
    
    vector<account_name> additional_users;
    auto add_users_num = 210 - _users.size();
    for (size_t u = 0; u < add_users_num; u++) {
        additional_users.push_back(user_name(u));
    }
    create_accounts(additional_users);
    _users.insert(_users.end(), additional_users.begin(), additional_users.end());
    
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

    for (size_t i = 0; i < _users.size(); i++) {
        BOOST_CHECK_EQUAL(success(), addvote(_users[i], {_users[0], "permlink"}, 10000));
        if (i % 10 == 0) {
            BOOST_TEST_MESSAGE("--- " << name{_users[i]}.to_string() << " voted; i = " << i);
            produce_block();
        }
    }
    BOOST_TEST_MESSAGE("--- all users have voted");
    produce_block();
    check();
    BOOST_CHECK_EQUAL(success(), vest.open(_stranger, _token_symbol, _stranger));
    BOOST_TEST_MESSAGE("--- _stranger opened an account");
    BOOST_CHECK_EQUAL(success(), addvote(_stranger, {_users[0], "permlink"}, 10000));
    BOOST_TEST_MESSAGE("--- _stranger voted");
    check();
    produce_blocks(golos::seconds_to_blocks(150));
    BOOST_CHECK_EQUAL(success(), post.closemssgs());
    check();
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(close_token_acc_test, reward_calcs_tester) try {
    BOOST_TEST_MESSAGE("close_token_acc_test");
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
    BOOST_TEST_MESSAGE("--- create_message: " << name{_users[0]}.to_string());
    BOOST_CHECK_EQUAL(success(), create_message({_users[0], "permlink"}));
    check();

    BOOST_TEST_MESSAGE("--- " << name{_users[1]}.to_string() << " voted");
    BOOST_CHECK_EQUAL(success(), addvote(_users[1], {_users[0], "permlink"}, 10000));
    check();

    BOOST_CHECK_EQUAL(success(), close_token_acc(_users[0], _token_symbol));
    BOOST_TEST_CHECK(token.get_account(_users[0]).is_null());

    auto forum_name_balance = token.get_account(_forum_name)["payments"].as<asset>();
    produce_blocks(golos::seconds_to_blocks(post.window));
    BOOST_CHECK_EQUAL(success(), post.closemssgs());
    produce_block();
    BOOST_CHECK_GT(token.get_account(_forum_name)["payments"].as<asset>(), forum_name_balance); 
    check();
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(close_vest_acc_test, reward_calcs_tester) try {
    BOOST_TEST_MESSAGE("close_vest_acc_test");
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
    BOOST_TEST_MESSAGE("--- create_message: " << name{_users[0]}.to_string());
    BOOST_CHECK_EQUAL(success(), create_message({_users[0], "permlink"}));
    check();

    BOOST_TEST_MESSAGE("--- " << name{_users[1]}.to_string() << " voted");
    BOOST_CHECK_EQUAL(success(), addvote(_users[1], {_users[0], "permlink"}, 10000));
    check();

    BOOST_CHECK_EQUAL(success(), retire_vest(vest.get_balance_raw(_users[0])["vesting"].as<asset>(), _users[0], _issuer));
    check();

    BOOST_CHECK_EQUAL(success(), close_vest_acc(_users[0], _token_symbol));
    BOOST_TEST_CHECK(vest.get_balance_raw(_users[0]).is_null());

    auto forum_name_balance = token.get_account(_forum_name)["payments"].as<asset>();
    produce_blocks(golos::seconds_to_blocks(post.window));
    BOOST_CHECK_EQUAL(success(), post.closemssgs());
    produce_block();
    BOOST_CHECK_GT(token.get_account(_forum_name)["payments"].as<asset>(), forum_name_balance); 
    check();
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(golos_delegators_test, reward_calcs_tester) try {
    BOOST_TEST_MESSAGE("golos_delegators_test");
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

    const uint8_t withdraw_intervals = 1;
    const uint32_t withdraw_interval_seconds = 12;
    const uint64_t vesting_min_amount = 0;
    const uint64_t delegation_min_amount = 5e6;
    const uint64_t delegation_min_remainder = 5e3;
    const uint32_t delegation_min_time = 0;
    const uint32_t delegation_return_time = 120;

    auto vesting_withdraw = vest.withdraw_param(withdraw_intervals, withdraw_interval_seconds);
    auto vesting_amount = vest.amount_param(vesting_min_amount);
    auto delegation = vest.delegation_param(delegation_min_amount, delegation_min_remainder, delegation_min_time, delegation_return_time);

    auto params = "[" + vesting_withdraw + "," + vesting_amount + "," + delegation + "]";
    BOOST_CHECK_EQUAL(success(), vest.set_params(_issuer, _token_symbol, params));

    const int divider = vest.make_asset(1).get_amount();
    const int min_remainder = delegation_min_remainder / divider;
    const auto amount = vest.make_asset(min_remainder);
    auto vest_amount = vest.make_asset(150000000);
    uint16_t interest_rate = 3000;
    
    BOOST_TEST_MESSAGE("--- one delegator interest_rate > 0");
    BOOST_TEST_MESSAGE("--- create_message: " << name{_users[0]}.to_string());
    BOOST_CHECK_EQUAL(success(), create_message({_users[0], "permlink"}));
    check();
    BOOST_CHECK_EQUAL(success(), delegate_vest(_users[2], _users[18], amount, interest_rate));
    check();
    BOOST_TEST_MESSAGE("--- " << name{_users[18]}.to_string() << " voted");
    BOOST_CHECK_EQUAL(success(), addvote(_users[18], {_users[0], "permlink"}, cfg::_100percent));
    check();
    auto voter_amount = vest.get_balance_raw(_users[18])["vesting"].as<asset>();
    auto delegator_amount = vest.get_balance_raw(_users[2])["vesting"].as<asset>();
    produce_blocks(golos::seconds_to_blocks(post.window));
    BOOST_CHECK_EQUAL(success(), post.closemssgs());
    produce_block();
    BOOST_CHECK_GT(vest.get_balance_raw(_users[18])["vesting"].as<asset>(), voter_amount);
    BOOST_CHECK_GT(vest.get_balance_raw(_users[2])["vesting"].as<asset>(), delegator_amount);
    check();

    BOOST_TEST_MESSAGE("--- add_funds_to_forum");
    BOOST_CHECK_EQUAL(success(), add_funds_to_forum(50000));
    check();

    BOOST_TEST_MESSAGE("--- one delegator interest_rate == 100");
    BOOST_TEST_MESSAGE("--- create_message: " << name{_users[0]}.to_string());
    BOOST_CHECK_EQUAL(success(), create_message({_users[0], "permlink1"}));
    check();
    BOOST_CHECK_EQUAL(success(), delegate_vest(_users[3], _users[19], amount, cfg::_100percent));
    check();
    BOOST_TEST_MESSAGE("--- " << name{_users[19]}.to_string() << " voted");
    BOOST_CHECK_EQUAL(success(), addvote(_users[19], {_users[0], "permlink1"}, cfg::_100percent));
    check();
    voter_amount = vest.get_balance_raw(_users[19])["vesting"].as<asset>();
    delegator_amount = vest.get_balance_raw(_users[3])["vesting"].as<asset>();
    produce_blocks(golos::seconds_to_blocks(post.window));
    BOOST_CHECK_EQUAL(success(), post.closemssgs());
    produce_block();
    BOOST_CHECK_GT(vest.get_balance_raw(_users[19])["vesting"].as<asset>(), voter_amount);
    BOOST_CHECK_GT(vest.get_balance_raw(_users[3])["vesting"].as<asset>(), delegator_amount);
    check();

    BOOST_TEST_MESSAGE("--- add_funds_to_forum");
    BOOST_CHECK_EQUAL(success(), add_funds_to_forum(50000));
    check();

    BOOST_TEST_MESSAGE("--- one delegator interest_rate == 0");
    BOOST_TEST_MESSAGE("--- create_message: " << name{_users[0]}.to_string());
    BOOST_CHECK_EQUAL(success(), create_message({_users[0], "permlink2"}));
    check();
    BOOST_CHECK_EQUAL(success(), delegate_vest(_users[4], _users[20], amount, 0));
    check();
    BOOST_TEST_MESSAGE("--- " << name{_users[20]}.to_string() << " voted");
    BOOST_CHECK_EQUAL(success(), addvote(_users[20], {_users[0], "permlink2"}, cfg::_100percent));
    check();
    voter_amount = vest.get_balance_raw(_users[20])["vesting"].as<asset>();
    delegator_amount = vest.get_balance_raw(_users[4])["vesting"].as<asset>();
    produce_blocks(golos::seconds_to_blocks(post.window));
    BOOST_CHECK_EQUAL(success(), post.closemssgs());
    produce_block();
    BOOST_CHECK_GT(vest.get_balance_raw(_users[20])["vesting"].as<asset>(), voter_amount);
    BOOST_CHECK_EQUAL(vest.get_balance_raw(_users[4])["vesting"].as<asset>(), delegator_amount);
    check();
    
    BOOST_TEST_MESSAGE("--- add_funds_to_forum");
    BOOST_CHECK_EQUAL(success(), add_funds_to_forum(50000));
    check();

    BOOST_TEST_MESSAGE("--- several delegators interest_rate > 0");
    BOOST_TEST_MESSAGE("--- create_message: " << name{_users[0]}.to_string());
    BOOST_CHECK_EQUAL(success(), create_message({_users[0], "permlink3"}));
    check();
    interest_rate = 4500;
    BOOST_CHECK_EQUAL(success(), delegate_vest(_users[5], _users[21], amount, interest_rate));
    BOOST_CHECK_EQUAL(success(), delegate_vest(_users[6], _users[21], amount, interest_rate));
    BOOST_CHECK_EQUAL(success(), delegate_vest(_users[7], _users[21], amount, interest_rate));
    check();
    BOOST_TEST_MESSAGE("--- " << name{_users[21]}.to_string() << " voted");
    BOOST_CHECK_EQUAL(success(), addvote(_users[21], {_users[0], "permlink3"}, cfg::_100percent));
    check();
    voter_amount = vest.get_balance_raw(_users[21])["vesting"].as<asset>();
    auto delegator1_amount = vest.get_balance_raw(_users[5])["vesting"].as<asset>();
    auto delegator2_amount = vest.get_balance_raw(_users[6])["vesting"].as<asset>();
    auto delegator3_amount = vest.get_balance_raw(_users[7])["vesting"].as<asset>();
    produce_blocks(golos::seconds_to_blocks(post.window));
    BOOST_CHECK_EQUAL(success(), post.closemssgs());
    produce_block();
    BOOST_CHECK_GT(vest.get_balance_raw(_users[21])["vesting"].as<asset>(), voter_amount);
    BOOST_CHECK_GT(vest.get_balance_raw(_users[5])["vesting"].as<asset>(), delegator1_amount);
    BOOST_CHECK_GT(vest.get_balance_raw(_users[6])["vesting"].as<asset>(), delegator2_amount);
    BOOST_CHECK_GT(vest.get_balance_raw(_users[7])["vesting"].as<asset>(), delegator3_amount);
    check();

    BOOST_TEST_MESSAGE("--- add_funds_to_forum");
    BOOST_CHECK_EQUAL(success(), add_funds_to_forum(50000));
    check();

    BOOST_TEST_MESSAGE("--- several delegators interest_rate > 0 with one interest_rate == 0");
    BOOST_TEST_MESSAGE("--- create_message: " << name{_users[0]}.to_string());
    BOOST_CHECK_EQUAL(success(), create_message({_users[0], "permlink4"}));
    check();
    BOOST_CHECK_EQUAL(success(), delegate_vest(_users[8], _users[22], amount, interest_rate));
    BOOST_CHECK_EQUAL(success(), delegate_vest(_users[9], _users[22], amount, interest_rate));
    BOOST_CHECK_EQUAL(success(), delegate_vest(_users[10], _users[22], amount, 0));
    check();
    BOOST_TEST_MESSAGE("--- " << name{_users[22]}.to_string() << " voted");
    BOOST_CHECK_EQUAL(success(), addvote(_users[22], {_users[0], "permlink4"}, cfg::_100percent));
    check();
    voter_amount = vest.get_balance_raw(_users[22])["vesting"].as<asset>();
    delegator1_amount = vest.get_balance_raw(_users[8])["vesting"].as<asset>();
    delegator2_amount = vest.get_balance_raw(_users[9])["vesting"].as<asset>();
    delegator3_amount = vest.get_balance_raw(_users[10])["vesting"].as<asset>();
    produce_blocks(golos::seconds_to_blocks(post.window));
    BOOST_CHECK_EQUAL(success(), post.closemssgs());
    produce_block();
    BOOST_CHECK_GT(vest.get_balance_raw(_users[22])["vesting"].as<asset>(), voter_amount);
    BOOST_CHECK_GT(vest.get_balance_raw(_users[8])["vesting"].as<asset>(), delegator1_amount);
    BOOST_CHECK_GT(vest.get_balance_raw(_users[9])["vesting"].as<asset>(), delegator2_amount);
    BOOST_CHECK_EQUAL(vest.get_balance_raw(_users[10])["vesting"].as<asset>(), delegator3_amount);
    check();

    BOOST_TEST_MESSAGE("--- add_funds_to_forum");
    BOOST_CHECK_EQUAL(success(), add_funds_to_forum(50000));
    check();
    
    BOOST_TEST_MESSAGE("--- checking that interest_rate < 1");
    BOOST_TEST_MESSAGE("--- create_message: " << name{_users[0]}.to_string());
    BOOST_CHECK_EQUAL(success(), create_message({_users[0], "permlink5"}));
    check();
    BOOST_CHECK_EQUAL(success(), delegate_vest(_users[11], _users[23], amount, cfg::_1percent));
    BOOST_CHECK_EQUAL(success(), delegate_vest(_users[12], _users[23], amount, cfg::_1percent));
    BOOST_CHECK_EQUAL(success(), delegate_vest(_users[13], _users[23], amount, interest_rate));
    check();
    BOOST_TEST_MESSAGE("--- " << name{_users[23]}.to_string() << " voted");
    BOOST_CHECK_EQUAL(success(), addvote(_users[23], {_users[0], "permlink5"}, cfg::_100percent));
    check();
    voter_amount = vest.get_balance_raw(_users[23])["vesting"].as<asset>();
    delegator1_amount = vest.get_balance_raw(_users[11])["vesting"].as<asset>();
    delegator2_amount = vest.get_balance_raw(_users[12])["vesting"].as<asset>();
    delegator3_amount = vest.get_balance_raw(_users[13])["vesting"].as<asset>();
    BOOST_CHECK_EQUAL(post.get_vote(_users[0], 0)["delegators"].size(), 1);
    produce_blocks(golos::seconds_to_blocks(post.window));
    BOOST_CHECK_EQUAL(success(), post.closemssgs());
    produce_block();
    std::vector<account_name> dlg_vec_db;
    std::vector<account_name> dlg_vec_st{_users[11], _users[12], _users[13]};
    for (auto dlg_obj : vest.get_delegators())
        if (dlg_obj["delegatee"].as<account_name>() == _users[23])
            dlg_vec_db.emplace_back(dlg_obj["delegator"].as<account_name>());
    BOOST_CHECK_EQUAL_COLLECTIONS(dlg_vec_st.begin(), dlg_vec_st.end(), dlg_vec_db.begin(), dlg_vec_db.end());
    /*
     * dlg_count = 3
     * dlg_amount = 5
     * voter_vest_amount = 500
     * effective_vesting = 500+5+5+5 = 515
     *
     * dlg_reward for _users[11] and _users[12]
     * interest_rate = 100
     * reward = 5 * 100 / 515 = 0.97
     *
     * dlg_reward for _users[13]
     * interest_rate = 4500
     * reward = 5 * 4500 / 515 = 43.689
     *
     * dlg_reward will be payed only for _users[13],
     * because 0.97 will be rounded to 0
    */
    BOOST_CHECK_GT(vest.get_balance_raw(_users[23])["vesting"].as<asset>(), voter_amount);
    BOOST_CHECK_EQUAL(vest.get_balance_raw(_users[11])["vesting"].as<asset>(), delegator1_amount);
    BOOST_CHECK_EQUAL(vest.get_balance_raw(_users[12])["vesting"].as<asset>(), delegator2_amount);
    BOOST_CHECK_GT(vest.get_balance_raw(_users[13])["vesting"].as<asset>(), delegator3_amount);
    check();

    BOOST_TEST_MESSAGE("--- add_funds_to_forum");
    BOOST_CHECK_EQUAL(success(), add_funds_to_forum(50000));
    check();

    BOOST_TEST_MESSAGE("--- increase vesting amount for " << name{_users[15]}.to_string());
    add_vesting({_users[15]}, vest_amount.get_amount());
    
    BOOST_TEST_MESSAGE("--- checking that interest_rate doesn't overflow and curator has no reward");
    BOOST_TEST_MESSAGE("--- create_message: " << name{_users[0]}.to_string());
    BOOST_CHECK_EQUAL(success(), create_message({_users[0], "permlink6"}));
    check();
    BOOST_TEST_MESSAGE("--- retire vesting for curator");
    BOOST_CHECK_EQUAL(success(), retire_vest(vest.get_balance_raw(_users[24])["vesting"].as<asset>(), _users[24], _issuer));
    BOOST_CHECK_EQUAL(success(), delegate_vest(_users[15], _users[24], vest_amount, cfg::_100percent));
    check();
    BOOST_TEST_MESSAGE("--- " << name{_users[24]}.to_string() << " voted");
    BOOST_CHECK_EQUAL(success(), addvote(_users[24], {_users[0], "permlink6"}, cfg::_100percent));
    check();
    voter_amount = vest.get_balance_raw(_users[24])["vesting"].as<asset>();
    delegator_amount = vest.get_balance_raw(_users[15])["vesting"].as<asset>();
    auto vote_dlg = post.get_vote(_users[0], 0)["delegators"];
    BOOST_CHECK_EQUAL(vote_dlg.size(), 1);
    BOOST_CHECK_EQUAL(vote_dlg[(size_t)0]["interest_rate"].as<uint16_t>(), cfg::_100percent);
    produce_blocks(golos::seconds_to_blocks(post.window));
    BOOST_CHECK_EQUAL(success(), post.closemssgs());
    produce_block();
    
    BOOST_CHECK_EQUAL(voter_amount.get_amount(), 0);
    BOOST_CHECK_EQUAL(vest.get_balance_raw(_users[24])["vesting"].as<asset>(), voter_amount);
    BOOST_CHECK_GT(vest.get_balance_raw(_users[15])["vesting"].as<asset>(), delegator_amount);
    check();
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(a_lot_of_delegators_test, reward_calcs_tester) try {

    BOOST_TEST_MESSAGE("a_lot_of_delegators_test");
    int64_t maxfp = std::numeric_limits<fixp_t>::max();
    auto bignum = 500000000000;
    
    vector<account_name> additional_users;
    size_t voters_num = 30;
    auto total_users_num = 210;
    auto add_users_num = total_users_num - _users.size();
    for (size_t u = 0; u < add_users_num; u++) {
        additional_users.push_back(user_name(u));
    }
    create_accounts(additional_users);
    _users.insert(_users.end(), additional_users.begin(), additional_users.end());
    auto vest_amount = 500000;
    init(bignum, vest_amount);
    produce_blocks();
    BOOST_TEST_MESSAGE("--- setrules");
    using namespace golos_curation;
    BOOST_CHECK_EQUAL(success(), setrules({"x", maxfp}, {golos_curation::func_str, maxfp}, {"1", bignum},
        [](double x){ return x; }, golos_curation::func, [](double x){ return 1.0; }));
    check();

    auto params = "[" + vest.withdraw_param(1, 12) + "," + vest.amount_param(0) + "," + vest.delegation_param(vest_amount, 1, 0, 120) + "]";
    BOOST_CHECK_EQUAL(success(), vest.set_params(_issuer, _token_symbol, params));

    BOOST_TEST_MESSAGE("--- add_funds_to_forum");
    BOOST_CHECK_EQUAL(success(), add_funds_to_forum(50000));
    
    for (size_t i = voters_num; i < total_users_num; i++) {
        produce_block();
        BOOST_CHECK_EQUAL(success(), delegate_vest(_users[i], _users[i % voters_num], asset(vest_amount, vest._symbol), cfg::_100percent));
    }
    check();
    BOOST_TEST_MESSAGE("--- create_message: " << name{_users[0]}.to_string());
    BOOST_CHECK_EQUAL(success(), create_message({_users[0], "permlink"}));
    check();

    for (size_t i = 0; i < voters_num; i++) {
        BOOST_CHECK_EQUAL(success(), addvote(_users[i], {_users[0], "permlink"}, 10000));
        BOOST_TEST_MESSAGE("--- " << name{_users[i]}.to_string() << " voted; i = " << i);
    }
    BOOST_TEST_MESSAGE("--- all users have voted");
    produce_blocks(golos::seconds_to_blocks(150));
    BOOST_CHECK_EQUAL(success(), post.closemssgs());
    check();
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(posting_bw_penalty, reward_calcs_tester) try {
    BOOST_TEST_MESSAGE("Posting bw penalty testing");
    int64_t maxfp = std::numeric_limits<fixp_t>::max();
    auto bignum = 500000000000;
    auto five_min = 5*60;
    auto window = 5000;
    auto pb_cutoff = cfg::_100percent*4;
    auto reward_weight_delta = 0.001;
    init(bignum, 500000);
    produce_blocks();
    
    auto params = "[" + post.get_str_cashout_window(window, post.upvote_lockout) + "]";
    BOOST_CHECK_EQUAL(success(), post.set_params(params));

    BOOST_TEST_MESSAGE("--- setrules");
    using namespace golos_curation;
    limitsarg lims = {{"t/300", "t/200", "t/(5*86400)", "t*p/86400"}, {{1, 0, cfg::_100percent, cfg::_100percent}, {2, 1, cfg::_100percent, cfg::_100percent/10}, {0, 2, cfg::_100percent, cfg::_100percent/(5*40)}, {3, 3, pb_cutoff, cfg::_100percent}}, {0, 0}, {0, 0, 0}};
    vector<limits::func_t> restorers_fn = {
            [](double p, double v, double t){ return t/300; },
            [](double p, double v, double t){ return t/200; }, 
            [](double p, double v, double t){ return t/(5*86400); },
            [](double p, double v, double t){ return t*p/86400; }
        };
    
    BOOST_CHECK_EQUAL(success(), setrules({"x", maxfp}, {golos_curation::func_str, maxfp}, {"x/1800", 1800},
        [](double x){ return x; }, golos_curation::func, [](double x){ return x / 1800.0; }, lims, std::move(restorers_fn)));
    check();
    
    BOOST_TEST_MESSAGE("--- create messages");
    auto charge = 0.0;
    auto charge_prev = 0.0;
    auto set_charge = [&]() {
        charge_prev = charge*five_min/86400;
        if (charge < charge_prev)
            charge = 0.0;
        charge -= charge_prev;
    };
    for (auto i = 0; i < 4; i++) {
        BOOST_TEST_MESSAGE("--- create_message: " << name{_users[0]}.to_string());
        set_charge();
        BOOST_CHECK_EQUAL(success(), create_message({_users[0], "permlink" + std::to_string(i)}));
        charge += cfg::_100percent;
        BOOST_TEST_CHECK(post.get_message(i+1)["rewardweight"].as<uint16_t>(), cfg::_100percent);
        produce_blocks(golos::seconds_to_blocks(five_min));
    }

    BOOST_TEST_MESSAGE("--- create_message: " << name{_users[0]}.to_string());
    set_charge();
    BOOST_CHECK_EQUAL(success(), create_message({_users[0], "permlink5"}));
    charge += cfg::_100percent;
    auto reward_weight_db = post.get_message(5)["rewardweight"].as<double>();
    double reward_weight_st = _state.pools.rbegin()->messages.back().reward_weight;
    BOOST_CHECK(reward_weight_db < cfg::_100percent);
    CHECK_EQUAL_WITH_DELTA(reward_weight_db/cfg::_100percent, reward_weight_st, reward_weight_delta);
    double rwrdwt = (pb_cutoff*pb_cutoff)/(charge*charge);
    CHECK_EQUAL_WITH_DELTA(reward_weight_db/cfg::_100percent, rwrdwt, reward_weight_delta);
    produce_blocks();

    produce_blocks(golos::seconds_to_blocks(window));
    BOOST_CHECK_EQUAL(success(), post.closemssgs());
    check();
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(message_max_payout_test, reward_calcs_tester) try {
    BOOST_TEST_MESSAGE("message_max_payout_test");
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

    auto create_msg = [&](mssgid message_id, optional<asset> max_payout = optional<asset>()) {
        return create_message(
            message_id,
            {N(), "parentprmlnk"},
            {},
            5000,
            false,
            "headermssg",
            "bodymssg",
            "languagemssg",
            {{"tag"}},
            "jsonmetadata",
            2500,
            max_payout
        );
    };

    BOOST_TEST_MESSAGE("--- create_message without max_payout: alice1");
    BOOST_CHECK_EQUAL(success(), create_msg({N(alice1), "permlink"}));
    check();

    BOOST_TEST_MESSAGE("--- create_message with max_payout = 0: alice2");
    BOOST_CHECK_EQUAL(success(), create_msg({N(alice2), "permlink"}, token.make_asset(0)));
    check();

    BOOST_TEST_MESSAGE("--- create_message with max_payout > prognosis payout: alice3");
    BOOST_CHECK_EQUAL(success(), create_msg({N(alice3), "permlink"}, token.make_asset(1000000)));
    check();

    BOOST_TEST_MESSAGE("--- create_message with max_payout = 0: alice4");
    BOOST_CHECK_EQUAL(success(), create_msg({N(alice4), "permlink"}, token.make_asset(0)));
    check();

    BOOST_TEST_MESSAGE("--- bob voted for all messages");
    BOOST_CHECK_EQUAL(success(), addvote(N(bob), {N(alice1), "permlink"}, 10000));
    BOOST_CHECK_EQUAL(success(), addvote(N(bob), {N(alice2), "permlink"}, 10000));
    BOOST_CHECK_EQUAL(success(), addvote(N(bob), {N(alice3), "permlink"}, 10000));
    BOOST_CHECK_EQUAL(success(), addvote(N(bob), {N(alice4), "permlink"}, 10000));
    produce_blocks();
    check();

    const auto alice1_vest = vest.get_balance_raw(N(alice1))["vesting"].as<asset>();
    const auto alice2_vest = vest.get_balance_raw(N(alice2))["vesting"].as<asset>();
    const auto alice3_vest = vest.get_balance_raw(N(alice3))["vesting"].as<asset>();
    const auto alice4_vest = vest.get_balance_raw(N(alice4))["vesting"].as<asset>();
    produce_blocks(golos::seconds_to_blocks(post.window));
    BOOST_CHECK_EQUAL(success(), post.closemssgs());
    BOOST_TEST_MESSAGE("--- rewards");
    check();
    show();

    BOOST_TEST_MESSAGE("--- checking that message without max_payout = 0 has payout");
    BOOST_CHECK_GT(vest.get_balance_raw(N(alice1))["vesting"].as<asset>() - alice1_vest, token.make_asset(0));

    BOOST_TEST_MESSAGE("--- checking that message with max_payout = 0 has no payout");
    BOOST_CHECK_EQUAL(vest.get_balance_raw(N(alice2))["vesting"].as<asset>() - alice2_vest, token.make_asset(0));

    BOOST_TEST_MESSAGE("--- checking that message with max_payout > prognosis_payout has payout");
    BOOST_CHECK_GT(vest.get_balance_raw(N(alice3))["vesting"].as<asset>() - alice3_vest, token.make_asset(0));

    BOOST_TEST_MESSAGE("--- checking that message with max_payout = 0, but last in pool, has payout");
    BOOST_CHECK_GT(vest.get_balance_raw(N(alice4))["vesting"].as<asset>() - alice4_vest, token.make_asset(0));
} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
