#define UNIT_TEST_ENV
//#define SHOW_ENABLE
#include "golos_tester.hpp"
#include "golos.vesting_test_api.hpp"
#include "eosio.token_test_api.hpp"
#include <math.h>
#include <map>
#include "contracts.hpp"
#include "../golos.publication/types.h"
#include "../golos.publication/config.hpp"
using namespace fixed_point_utils;
#define PRECESION 0             // why 0?
#define TOKEN_NAME "GOLOS"
constexpr int64_t MAXTOKENPROB = 5000;
constexpr auto MAX_ARG = static_cast<double>(std::numeric_limits<fixp_t>::max());

namespace cfg = golos::config;


struct balance_data {
    double tokenamount = 0.0;
    double vestamount = 0.0;
    //vesting delegation isn't covered by these tests
};

struct pool_data {
    double msgs = 0.0;
    double funds = 0.0;
    double rshares = 0.0;
    double rsharesfn = 0.0;
};

struct message_data {
    double absshares = 0.0;
    double netshares = 0.0;
    double voteshares = 0.0;
    double sumcuratorsw = 0.0;
};

constexpr struct {
    balance_data balance {10.0, 20.0};
    pool_data pool {0.001, 15, -0.01, -0.01};
    message_data message {-0.01, -0.01, -0.01, -0.01};
} delta;

constexpr double balance_delta = 0.01;
constexpr double funds_delta = 0.01;
constexpr double pool_rshares_delta = 0.01;
constexpr double pool_rsharesfn_delta = 0.01;

using namespace eosio::testing;
using namespace eosio;
using namespace eosio::chain;
using namespace fc;
using namespace std;

using mvo = fc::mutable_variant_object;

double log2(double arg) {
    return log(arg) / log(2.0);
}

double get_prop(int64_t arg) {
    return static_cast<double>(arg) / static_cast<double>(cfg::_100percent);
}

struct message_key {
    account_name author;
    uint64_t id;
    bool operator ==(const message_key& rhs) const {
        return (author == rhs.author) && (id == rhs.id);
    }
};

struct aprox_val_t {
    double val;
    double delta;
    bool operator ==(const aprox_val_t& rhs) const {
        BOOST_REQUIRE_MESSAGE(((rhs.delta >= 0.0) && (delta >= 0.0)) || ((rhs.delta <= 0.0) && (delta <= 0.0)), "aprox_val_t operator ==(): wrong comparison mode");
        if(((rhs.val > 0.0) && (val < 0.0)) || ((rhs.val < 0.0) && (val > 0.0)))
            return false;
        double d = std::min(double(std::abs(rhs.delta)), std::abs(double(delta)));
        if(delta < 0.0) {
            double a = std::min(std::abs(rhs.val), std::abs(val));
            double b = std::max(std::abs(rhs.val), std::abs(val));
            return (b < 1.e-20) || ((a / b) > (1.0 - d));
        }
        else
            return ( (val - d) <= rhs.val ) && ( rhs.val <= (val + d) );
    }

    aprox_val_t& operator = (const double &arg) { val = arg; return *this; }
    aprox_val_t& operator +=(const double &arg) { val += arg; return *this; }
};

std::ostream& operator<< (std::ostream& os, const aprox_val_t& rhs) {
    os << std::to_string(rhs.val) << " (" << std::to_string(double(rhs.delta)) << ") ";
    return os;
}

struct statemap : public std::map<std::string, aprox_val_t> {
    static std::string get_pool_str(uint64_t id) {
        return "pool #" +  std::to_string(id) + ": ";
    }
    static std::string get_balance_str(account_name acc) {
        return "balance of " +  acc.to_string() + ": ";
    }
    static std::string get_message_str(const message_key& msg) {
        return "message of " + msg.author.to_string() + " #" + std::to_string(msg.id) + ": ";
    }
    static std::string get_vote_str(account_name voter, const message_key& msg) {
        return "vote of " + voter.to_string() + " for message of " + msg.author.to_string() + " #" + std::to_string(msg.id) + ": ";
    }
    void set_pool(uint64_t id, const pool_data& data = {}) {
        auto prefix = statemap::get_pool_str(id);
        operator[](prefix + "msgs") = { data.msgs, delta.pool.msgs };
        operator[](prefix + "funds") = { data.funds, delta.pool.funds };
        operator[](prefix + "rshares") = { data.rshares, delta.pool.rshares };
        operator[](prefix + "rsharesfn") = { data.rsharesfn, delta.pool.rsharesfn };
    }
    void set_balance(account_name acc, const balance_data data = {}) {
        auto prefix = statemap::get_balance_str(acc);
        operator[](prefix + "tokenamount") = { data.tokenamount, delta.balance.tokenamount };
        operator[](prefix + "vestamount") = { data.vestamount, delta.balance.vestamount };
    }

    void set_balance_token(account_name acc, double val) {
        auto prefix = statemap::get_balance_str(acc);
        operator[](prefix + "tokenamount") = { val, delta.balance.tokenamount };
    }

    void set_balance_vesting(account_name acc, double val) {
        auto prefix = statemap::get_balance_str(acc);
        operator[](prefix + "vestamount") = { val, delta.balance.vestamount };
    }

    void set_message(const message_key& msg, const message_data& data = {}) {

        auto prefix = statemap::get_message_str(msg);
        operator[](prefix + "absshares") = { data.absshares, delta.message.absshares };
        operator[](prefix + "netshares") = { data.netshares, delta.message.netshares };
        operator[](prefix + "voteshares") = { data.voteshares, delta.message.voteshares };
        operator[](prefix + "sumcuratorsw") = { data.sumcuratorsw, delta.message.sumcuratorsw };
    }

    pool_data get_pool(uint64_t id) {
        auto prefix = statemap::get_pool_str(id);
        return {
            operator[](prefix + "msgs").val,
            operator[](prefix + "funds").val,
            operator[](prefix + "rshares").val,
            operator[](prefix + "rsharesfn").val
        };
    }

    void remove_same(const statemap& rhs) {
        for(auto itr_rhs = rhs.begin(); itr_rhs != rhs.end(); itr_rhs++) {
            auto itr = find(itr_rhs->first);
            if(itr->second == itr_rhs->second)
                erase(itr);
        }
    }
};

std::ostream& operator<< (std::ostream& os, const statemap& rhs) {
    for(auto itr = rhs.begin(); itr != rhs.end(); ++itr)
        os << itr->first << " = " << itr->second << "\n";
    return os;
}

class extended_tester : public golos_tester {
    fc::microseconds _cur_time;
    void update_cur_time() { _cur_time = control->head_block_time().time_since_epoch();};

protected:
    const fc::microseconds& cur_time()const { return _cur_time; };

public:

    void step(uint32_t n = 1) {
        produce_blocks(n);
        update_cur_time();
    }

    void run(const fc::microseconds& t) {
        _produce_block(t);
        update_cur_time();
    }
};

struct vote {
    account_name voter;
    double weight;
    double vesting;
    double created;//time
};

struct beneficiary {
    account_name account;
    int64_t deductprcnt;
};
FC_REFLECT(beneficiary, (account)(deductprcnt))

struct tags {
    std::string tag;
};
FC_REFLECT(tags, (tag))

struct message {
    message_key key;
    double tokenprop;
    std::list<vote> votes;
    double created;//time
    std::vector<beneficiary> beneficiaries;
    double reward_weight;
    double get_rshares_sum()const {
        double ret = 0.0;
        for(auto& v : votes)
            ret += v.weight * v.vesting;
        return ret;
    };
    message(message_key k, double tokenprop_, double created_, const std::vector<beneficiary>& beneficiaries_, double reward_weight_) :
        key(k), tokenprop(tokenprop_), created(created_), beneficiaries(beneficiaries_), reward_weight(reward_weight_) {};
};

class cutted_func {
    std::function<double(double)> _f;
    double _maxarg;
public:
    cutted_func(std::function<double(double)>&& f, double maxarg) : _f(std::move(f)), _maxarg(maxarg) {};
    double operator()(double arg)const {return _f(std::min(std::max(0.0, arg), _maxarg));};
};

struct rewardrules {
    cutted_func mainfunc;
    cutted_func curationfunc;
    cutted_func timepenalty;
    double curatorsprop;
    rewardrules(
        cutted_func&& mainfunc_,
        cutted_func&& curationfunc_,
        cutted_func&& timepenalty_,
        double curatorsprop_
    ) :
        mainfunc(std::move(mainfunc_)),
        curationfunc(std::move(curationfunc_)),
        timepenalty(std::move(timepenalty_)),
        curatorsprop(curatorsprop_) {};
};

struct chargeinfo {
    uint64_t lastupdate;
    double data;
};

using usercharges = std::vector<chargeinfo>;

struct limits {
    using func_t = std::function<double(double, double, double)>;
    enum kind_t: enum_t {POST, COMM, VOTE, POSTBW, UNDEF};
    std::vector<func_t> restorers; //(funcs of: prev_charge (p), vesting (v), elapsed_seconds (t))
    std::vector<limitedact> limitedacts;
    std::vector<int64_t> vestingprices;//disabled if < 0
    std::vector<int64_t> minvestings;

    const limitedact& get_limited_act(kind_t kind)const {
        return limitedacts.at(static_cast<size_t>(kind));
    }

    const func_t* get_restorer(kind_t kind)const {
        uint8_t num = get_limited_act(kind).restorernum;
        return (num == disabled_restorer) ? nullptr : &(restorers.at(num));
    }

    double get_vesting_price(kind_t kind)const {
        auto k = static_cast<size_t>(kind);
        return (k < vestingprices.size()) ? vestingprices[k] : -1.0;
    }

    double get_min_vesting_for(kind_t kind)const {
        return minvestings.at(static_cast<size_t>(kind));
    }

    size_t get_charges_num()const {
        size_t ret = 0;
        for(auto& act : limitedacts)
            ret = std::max(ret, static_cast<size_t>(act.chargenum) + 1);
        return ret;
    }

    bool check()const {
        if((vestingprices.size() >= 3) || (minvestings.size() != 3))
            return false;
        for(auto& act : limitedacts)
            if(!(act.restorernum < restorers.size()) || (act.restorernum == disabled_restorer))
                return false;
        return true;
    }

    void update_charge(chargeinfo& chg, const func_t* restorer, uint64_t cur_time, double vesting, double price, double w = 1.0) const {
        using namespace cfg;    // for limit_restorer_domain
        if (restorer != nullptr) {
            auto prev = std::min(chg.data, static_cast<double>(limit_restorer_domain::max_prev));
            auto v_corr = std::min(vesting, static_cast<double>(limit_restorer_domain::max_vesting));
            auto elapsed_seconds = std::min(static_cast<double>((cur_time - chg.lastupdate) / seconds(1).count()),
                static_cast<double>(limit_restorer_domain::max_elapsed_seconds));
            auto restored = (*restorer)(prev, v_corr, elapsed_seconds);
            if (restored > prev)
                chg.data = 0.0;
            else
                chg.data = std::min(chg.data - restored, static_cast<double>(limit_restorer_domain::max_res));
        }
        if(price > 0.0) {
            auto added = std::max(price * w, static_cast<double>(std::numeric_limits<fixp_t>::min()));
            chg.data = std::min(chg.data + added, static_cast<double>(limit_restorer_domain::max_res));
        }
        chg.lastupdate = cur_time;
    }
    double calc_power(limits::kind_t kind, usercharges& charges, uint64_t cur_time, double vesting, double w)const {
    // charges are not const here
        auto& lim_act = get_limited_act(kind);
        auto restorer = get_restorer(kind);
        auto& chg = charges[lim_act.chargenum];
        update_charge(chg, restorer, cur_time, vesting, get_prop(lim_act.chargeprice), w);
        return (chg.data > get_prop(lim_act.cutoffval)) ? (get_prop(lim_act.cutoffval) / chg.data) : 1.0;
    }

    double apply(kind_t kind, usercharges& charges, double& vesting_ref, uint64_t cur_time, double w)const {
        if((kind == limits::VOTE) && (w != 0)) {
            if((w * vesting_ref) < get_min_vesting_for(kind))
                return -1.0;
        }

        else if((kind != limits::VOTE) && (vesting_ref < get_min_vesting_for(kind)))
            return -1.0;

        auto charges_prev = charges;
        if(charges.empty())
            charges.resize(get_charges_num(), {cur_time, 0});
        auto power = calc_power(kind, charges, cur_time, vesting_ref, (kind == limits::VOTE) ? w : 1.0);
        if(power != 1.0)
            charges = charges_prev;

        if(kind == limits::VOTE)
            return (power == 1.0) ? 1.0 : -1.0;

        if((power != 1.0) && (w == 0.0))
            return -1.0;

        if(power != 1.0) {//=> w != 0
            auto price = get_vesting_price(kind);
            if((price <= 0) || (price > vesting_ref))
                return -1.0;
            vesting_ref -= price;
        }
        return calc_power(limits::POSTBW, charges, cur_time, vesting_ref, 1.0);
    }
};

struct rewardpool {
    uint64_t id;
    double funds;
    rewardrules rules;
    limits lims;
    std::list<message> messages;
    std::map<account_name, usercharges> charges;

    double get_rshares_sum()const {
        double ret = 0.0;
        for (auto& m : messages)
            ret += m.get_rshares_sum();
        return ret;
    }

    //yup, it's dumb. but we are testing here, we don't need performance, we prefer straightforward logic
    double get_rsharesfn_sum()const {
        double ret = 0.0;
        for (auto& m : messages)
            ret += rules.mainfunc(m.get_rshares_sum());
        return ret;
    }

    double get_ratio()const {
        BOOST_REQUIRE_MESSAGE(funds >= 0.0, "rewardpool: wrong payment mode");
        auto r = get_rshares_sum();
        return (r > 0) ? (funds / r) : std::numeric_limits<double>::max();
    }
    rewardpool(uint64_t id_, rewardrules&& rules_, limits&& lims_) : id(id_), funds(0.0), rules(std::move(rules_)), lims(std::move(lims_)) {};

};

struct state {
    std::map<account_name, balance_data> balances;
    std::vector<rewardpool> pools;
    void clear() { balances.clear(); pools.clear(); };
};


class reward_calcs_tester : public extended_tester {
    symbol _token_symbol;
    golos_vesting_api vest;
    eosio_token_api token;

protected:
    account_name _forum_name;
    account_name _issuer;
    std::vector<account_name> _users;
    account_name _stranger;
    statemap _req;
    statemap _res;
    state _state;

    void init(int64_t issuer_funds, int64_t user_vesting_funds) {
        BOOST_CHECK_EQUAL(success(),
            token.create(_issuer, asset(issuer_funds + _users.size() * user_vesting_funds, _token_symbol)));
        step();

        BOOST_CHECK_EQUAL(success(), token.issue(_issuer, _issuer, asset(issuer_funds, _token_symbol), "HERE COULD BE YOUR ADVERTISEMENT"));
        step();

        BOOST_CHECK_EQUAL(success(), token.open(_forum_name, _token_symbol, _forum_name));
        BOOST_CHECK_EQUAL(success(), vest.create_vesting(_issuer, _token_symbol, {_issuer, _forum_name}));
        step();

        BOOST_CHECK_EQUAL(success(), vest.open(_forum_name, _token_symbol, _forum_name));
        for (auto& u : _users) {
            BOOST_CHECK_EQUAL(success(), vest.open(u, _token_symbol, u));
            step();
            BOOST_CHECK_EQUAL(success(), add_funds_to(u, user_vesting_funds));
            step();
            BOOST_CHECK_EQUAL(success(), buy_vesting_for(u, user_vesting_funds));
            step();
        }
        check();
        _req.clear();
        _res.clear();
    }

public:
    reward_calcs_tester()
        : _token_symbol(PRECESION, TOKEN_NAME)
        , vest({this, cfg::vesting_name, _token_symbol})
        , token({this, cfg::token_name, _token_symbol})

        , _forum_name(N(reward.calcs))
        , _issuer(N(issuer.acc))
        , _users{
                N(alice),  N(alice1),  N(alice2), N(alice3), N(alice4), N(alice5),
                N(bob), N(bob1), N(bob2), N(bob3), N(bob4), N(bob5),
                N(why), N(has), N(my), N(imagination), N(become), N(poor)}
        , _stranger(N(dan.larimer)) {

        step(2);
        create_accounts({_forum_name, _issuer, cfg::vesting_name, cfg::token_name, cfg::control_name, _stranger});
        create_accounts(_users);
        step(2);

        install_contract(_forum_name, contracts::posting_wasm(), contracts::posting_abi());
        install_contract(cfg::token_name, contracts::token_wasm(), contracts::token_abi());
        install_contract(cfg::vesting_name, contracts::vesting_wasm(), contracts::vesting_abi());
        // install_contract(cfg::control_name, contracts::ctrl_wasm(), contracts::ctrl_abi());
    }

    action_result add_funds_to(account_name user, int64_t amount) {
        auto ret = token.transfer(_issuer, user, asset(amount, _token_symbol));
        if (ret == success())
            _state.balances[user].tokenamount  += amount;
        return ret;
    }

    double convert_to_vesting(double arg) {
        double tokn_total = token.get_account(cfg::vesting_name)["balance"].as<asset>().to_real();
        double vest_total = vest.get_vesting()["supply"].as<asset>().to_real();
        double price = (vest_total < 1.e-20) || (tokn_total < 1.e-20) ? 1.0 : (vest_total / tokn_total);
        return arg * price;
    }

    action_result buy_vesting_for(account_name user, int64_t amount) {
        auto ret = token.transfer(user, cfg::vesting_name, asset(amount, _token_symbol));
        if (ret == success()) {
            _state.balances[user].tokenamount -= amount;
            _state.balances[user].vestamount += convert_to_vesting(amount);
            BOOST_CHECK_EQUAL(success(), vest.unlock_limit(user, asset(amount*10, _token_symbol)));
        }
        return ret;
    }

    void fill_depleted_pool(int64_t amount, size_t excluded) {
        size_t choice = 0;
        auto min_ratio = std::numeric_limits<double>::max();
        for(size_t i = 0; i < _state.pools.size(); i++)
            if(i != excluded) {
                double cur_ratio = _state.pools[i].get_ratio();
                if(cur_ratio <= min_ratio) {
                    min_ratio = cur_ratio;
                    choice = i;
                }
            }
        _state.pools[choice].funds += amount;
    }

    action_result add_funds_to_forum(int64_t amount) {
        auto ret = token.transfer(_issuer, _forum_name, asset(amount, _token_symbol));
        if (ret == success()) {
            BOOST_REQUIRE_MESSAGE(!_state.pools.empty(), "issue: _state.pools is empty");
            fill_depleted_pool(amount, _state.pools.size());
        }
        return ret;
    }

    action_result push_action(const account_name& signer, const action_name& name, const variant_object& data, const account_name& code) {
        return golos_tester::push_action(code, name, signer, data);
    }

    action_result setrules(
        const funcparams& mainfunc,
        const funcparams& curationfunc,
        const funcparams& timepenalty,
        int64_t curatorsprop,
        std::function<double(double)>&& mainfunc_,
        std::function<double(double)>&& curationfunc_,
        std::function<double(double)>&& timepenalty_,
        const limitsarg& lims = {{"0"}, {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}, {0, 0}, {0, 0, 0}},
        std::vector<limits::func_t>&& func_restorers = {
            [](double, double, double){ return 0.0; }
        }
    ) {
        auto ret = push_action(
            _forum_name,
            N(setrules),
            mvo()
                ("mainfunc", mvo()("str", mainfunc.str)("maxarg", mainfunc.maxarg))
                ("curationfunc", mvo()("str", curationfunc.str)("maxarg", curationfunc.maxarg))
                ("timepenalty", mvo()("str", timepenalty.str)("maxarg", timepenalty.maxarg))
                ("curatorsprop", curatorsprop)
                ("maxtokenprop", MAXTOKENPROB)
                ("tokensymbol", _token_symbol)
                ("lims", lims),
            _forum_name
        );
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

            auto rows = get_all_chaindb_rows(_forum_name, _forum_name, N(rewardpools), false);
            auto created = (*rows.rbegin())["created"].as<uint64_t>();
            _state.pools.emplace_back(rewardpool(created,
                rewardrules(
                    {std::move(mainfunc_), static_cast<double>(mainfunc.maxarg)},
                    {std::move(curationfunc_), static_cast<double>(curationfunc.maxarg)},
                    {std::move(timepenalty_), static_cast<double>(timepenalty.maxarg)},
                    static_cast<double>(curatorsprop) / static_cast<double>(cfg::_100percent)
                ),
                limits{
                    .restorers = std::move(func_restorers),
                    .limitedacts = lims.limitedacts,
                    .vestingprices = lims.vestingprices,
                    .minvestings = lims.minvestings
                }));
            _state.pools.back().funds += unclaimed_funds;
        }
        return ret;
    }

    void set_token_balance_from_table(account_name user, statemap& s) {
        auto rows = get_all_chaindb_rows(N(eosio.token), user, N(accounts), false);
        BOOST_CHECK_EQUAL(rows.size() <= 1, true); //for now we are checking just one token
        if (rows.empty())
            s.set_balance_token(user, 0.0);
        else {
            auto cur = *rows.begin();
            s.set_balance_token(user, static_cast<double>(cur["balance"].as<asset>().to_real()));
        }
    }

    void set_vesting_balance_from_table(account_name user, statemap& s) {
        auto rows = get_all_chaindb_rows(cfg::vesting_name, user, N(balances), false);
        BOOST_CHECK_EQUAL(rows.size() <= 1, true); //--/---/---/---/---/---/ just one vesting
        if (rows.empty())
            s.set_balance_vesting(user, 0.0);
        else {
            auto cur = *rows.begin();
            s.set_balance_vesting(user, static_cast<double>(cur["vesting"].as<asset>().to_real()));
        }
    }

    void fill_from_state(statemap& s) const {
        s.clear();
        for(auto& b : _state.balances)
            s.set_balance(b.first, {b.second.tokenamount, b.second.vestamount});

        for(auto& p : _state.pools) {
            double pool_rshares_sum = 0.0;
            double pool_rsharesfn_sum = 0.0;

            for(auto& m : p.messages) {
                double absshares = 0.0;
                double netshares = 0.0;
                double voteshares = 0.0;
                for(auto& v : m.votes) {
                    absshares +=  std::abs(v.weight) * v.vesting;
                    double currshares = v.weight * v.vesting;
                    netshares += currshares;
                    if(v.weight > 0.0)
                        voteshares += currshares;
                }
                absshares  = std::min(MAX_ARG, absshares);
                netshares  = std::min(MAX_ARG, netshares);
                voteshares = std::min(MAX_ARG, voteshares);
                s.set_message(m.key, {absshares, netshares, voteshares, p.rules.curationfunc(voteshares)});
                pool_rshares_sum = std::min(MAX_ARG, netshares + pool_rshares_sum);
                pool_rsharesfn_sum += p.rules.mainfunc(netshares);
            }
            s.set_pool(p.id, {static_cast<double>(p.messages.size()), p.funds, pool_rshares_sum, pool_rsharesfn_sum});
        }
    }

    void fill_from_tables(statemap& s) {
        s.clear();

        for(auto& user : _users) {
            set_token_balance_from_table(user, s);
            set_vesting_balance_from_table(user, s);

        }

        //pools
        {
            auto rows = get_all_chaindb_rows(_forum_name, _forum_name, N(rewardpools), false);
            for (auto itr = rows.begin(); itr != rows.end(); ++itr) {
                auto cur = *itr;
                s.set_pool(cur["created"].as<uint64_t>(), {
                    static_cast<double>(cur["state"]["msgs"].as<uint64_t>()),
                    static_cast<double>(cur["state"]["funds"].as<eosio::chain::asset>().to_real()),
                    static_cast<double>(FP(cur["state"]["rshares"].as<base_t>())),
                    static_cast<double>(FP(cur["state"]["rsharesfn"].as<base_t>()))
                });
            }
        }

        //messages
        {
            for(auto& user : _users) {
                auto rows = get_all_chaindb_rows(_forum_name, user, N(messagetable), false);
                for (auto itr = rows.begin(); itr != rows.end(); ++itr) {
                    auto cur = *itr;
                    if (!cur["closed"].as<bool>())
                        s.set_message(message_key{user, cur["id"].as<uint64_t>()}, {
                            static_cast<double>(FP(cur["state"]["absshares"].as<base_t>())),
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
        if(req) {
            fill_from_state(_req);
            BOOST_TEST_MESSAGE("_req:\n" << _req);
        }
        if(res) {
             fill_from_tables(_res);
             BOOST_TEST_MESSAGE("_res :\n" << _res);
        }
#endif
    }
    void pay_rewards_in_state() {
        for(auto itr_p = _state.pools.begin(); itr_p != _state.pools.end(); itr_p++) {
            auto& p = *itr_p;
            for(auto itr_m = p.messages.begin(); itr_m != p.messages.end();) {
                if((cur_time().to_seconds() - itr_m->created) > CLOSE_MESSAGE_PERIOD) {
                    auto m = *itr_m;
                    double pool_rsharesfn_sum = p.get_rsharesfn_sum();

                    int64_t payout = 0;
                    if (p.messages.size() == 1) {
                        payout = p.funds;
                    } else if (pool_rsharesfn_sum > 1.e-20) {
                        payout = (p.rules.mainfunc(m.get_rshares_sum()) * p.funds) / pool_rsharesfn_sum;
                        payout *= m.reward_weight;
                    }

                    auto curation_payout = p.rules.curatorsprop * payout;
                    double unclaimed_funds = curation_payout;
                    std::list<std::pair<account_name, double>> cur_rewards;
                    double curators_fn_sum = 0.0;
                    double rshares_sum = 0.0;
                    for(auto& v : m.votes) {
                        if(v.weight > 0.0)
                          rshares_sum += v.weight * v.vesting;
                        double new_cur_fn = p.rules.curationfunc(rshares_sum);
                        cur_rewards.emplace_back(std::make_pair(v.voter, (new_cur_fn - curators_fn_sum) *
                            std::min(p.rules.timepenalty(v.created - m.created), 1.0)
                        ));
                        curators_fn_sum = new_cur_fn;
                    }
                    if(curators_fn_sum > 1.e-20)
                        for(auto& r : cur_rewards)
                            r.second *= (curation_payout / curators_fn_sum);

                    for(auto& r : cur_rewards) {
                        _state.balances[r.first].vestamount += convert_to_vesting(r.second);
                        unclaimed_funds -= r.second;
                    }

                    auto author_payout =  payout - curation_payout;
                    double ben_payout_sum = 0.0;
                    for(auto& ben : m.beneficiaries) {
                        double ben_payout = author_payout * get_prop(ben.deductprcnt);
                        _state.balances[ben.account].vestamount += convert_to_vesting(ben_payout);
                        ben_payout_sum += ben_payout;
                    }
                    author_payout -= ben_payout_sum;

                    _state.balances[m.key.author].tokenamount += author_payout * m.tokenprop;
                    _state.balances[m.key.author].vestamount += convert_to_vesting(author_payout * (1.0 - m.tokenprop));

                    p.messages.erase(itr_m++);
                    p.funds -= (payout - unclaimed_funds);

                    if(p.messages.size() == 0) {
                        auto itr_p_next = itr_p;
                        if((++itr_p_next) != _state.pools.end()) {
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

    void check(const std::string& s = std::string()) {
        pay_rewards_in_state();

        if(!s.empty())
            BOOST_TEST_MESSAGE(s);
        step();
        fill_from_tables(_res);
        fill_from_state(_req);

        _res.remove_same(_req);
        if(!_res.empty()){
            BOOST_TEST_MESSAGE( "_res != _req\n diff: " );
            BOOST_TEST_MESSAGE(_res);
            BOOST_TEST_MESSAGE( "_req:" );
            BOOST_TEST_MESSAGE(_req);
            BOOST_REQUIRE(false);
        }
    }

    action_result create_message(account_name account, std::string permlink,
        account_name parentacc = N(), std::string parentprmlnk = "parentprmlnk",
        std::vector<beneficiary> beneficiaries = {}, int64_t tokenprop = 5000,
        bool vestpayment = false,
        std::string headermssg = "headermssg",
        std::string bodymssg = "bodymssg",  std::string languagemssg = "languagemssg",
        std::vector<tags> tags = {{"tag"}}, std::string jsonmetadata = "jsonmetadata") {

        auto ret = push_action(account, N(createmssg),
            mvo()
               ("account", account)
               ("permlink", permlink)
               ("parentacc", parentacc)
               ("parentprmlnk", parentprmlnk)
               ("beneficiaries", beneficiaries)
               ("tokenprop", tokenprop)
               ("vestpayment", vestpayment)
               ("headermssg", headermssg)
               ("bodymssg", bodymssg)
               ("languagemssg", languagemssg)
               ("tags", tags)
               ("jsonmetadata", jsonmetadata),
            _forum_name);
        message_key key{account, hash64(permlink)};

        auto reward_weight = 0.0;
        std::string ret_str = ret;
        if((ret == success()) || (ret_str.find("forum::apply_limits:") != std::string::npos)) {
            reward_weight = _state.pools.back().lims.apply(
                    parentacc ? limits::COMM : limits::POST,
                    _state.pools.back().charges[key.author],
                    _state.balances[key.author].vestamount,
                    cur_time().count(),
                    vestpayment);
            BOOST_REQUIRE_MESSAGE(((ret == success()) == (reward_weight >= 0.0)), "wrong ret_str: " + ret_str
                + "; vesting = " + std::to_string(_state.balances[key.author].vestamount)
                + "; reward_weight = " + std::to_string(reward_weight));
        }

        if(ret == success()) {
            _state.pools.back().messages.emplace_back(message(
                key,
                static_cast<double>(std::min(tokenprop, MAXTOKENPROB)) / static_cast<double>(cfg::_100percent),
                static_cast<double>(cur_time().to_seconds()),
                beneficiaries, reward_weight));
        } else {
            key.id = 0;
        }
        return ret;
    }

    action_result addvote(account_name voter, account_name author, std::string permlink, int64_t weight) {
        auto act = N(upvote);
        int16_t w = weight;
        if(w == 0)
            act = N(unvote);
        if(w < 0) {
            act = N(downvote);
            w = -w;
        }
        auto ret = push_action(voter, act, mvo()
            ("voter", voter)
            ("author", author)
            ("permlink", permlink)
            ("weight", w), _forum_name);
        message_key msg_key{author, hash64(permlink)};

        std::string ret_str = ret;
        if((ret == success()) || (ret_str.find("forum::apply_limits:") != std::string::npos)) {
            auto apply_ret = _state.pools.back().lims.apply(
                    limits::VOTE, _state.pools.back().charges[voter],
                    _state.balances[voter].vestamount, cur_time().count(), get_prop(std::abs(weight)));
            BOOST_REQUIRE_MESSAGE(((ret == success()) == (apply_ret >= 0.0)), "wrong ret_str: " + ret_str
                + "; vesting = " + std::to_string(_state.balances[voter].vestamount)
                + "; apply_ret = " + std::to_string(apply_ret));
        }

        if(ret == success()) {
            for(auto& p : _state.pools)
                for(auto& m : p.messages)
                    if(m.key == msg_key) {
                        m.votes.emplace_back(vote{
                            voter,
                            std::min(static_cast<double>(weight) / static_cast<double>(cfg::_100percent), 1.0),
                            _state.balances[voter].vestamount,
                            static_cast<double>(cur_time().to_seconds())
                        });
                        return ret;
                    }
            BOOST_REQUIRE_MESSAGE(false, "addvote: ret == success(), but message not found in state");
        }
        return ret;
    }
};

BOOST_AUTO_TEST_SUITE(reward_calcs_tests)

BOOST_FIXTURE_TEST_CASE(basic_tests, reward_calcs_tester) try {
    BOOST_TEST_MESSAGE("Basic publication_rewards tests");
    auto bignum = 500000000000;
    init(bignum, 500000);
    step();
    BOOST_TEST_MESSAGE("--- setrules");
    BOOST_CHECK_EQUAL("assertion failure with message: check monotonic failed for time penalty func",
        setrules({"x", bignum}, {"log2(x + 1.0)", bignum}, {"1-x", bignum}, 2500,
        [](double x){ return x; }, [](double x){ return log2(x + 1.0); }, [](double x){ return 1.0-x; }));

    BOOST_CHECK_EQUAL("assertion failure with message: fp_cast: overflow",
        setrules({"x", std::numeric_limits<base_t>::max()}, {"sqrt(x)", bignum}, {"1", bignum}, 2500,
        [](double x){ return x; }, [](double x){ return sqrt(x); }, [](double x){ return 1.0; }));

    BOOST_CHECK_EQUAL("assertion failure with message: check positive failed for time penalty func",
        setrules({"x", bignum}, {"sqrt(x)", bignum}, {"-10.0+x", 15}, 2500,
        [](double x){ return x; }, [](double x){ return sqrt(x); }, [](double x){ return -10.0 + x; }));

    BOOST_CHECK_EQUAL(success(), setrules({"x^2", static_cast<base_t>(sqrt(bignum))}, {"sqrt(x)", bignum}, {"1", bignum}, 2500,
        [](double x){ return x * x; }, [](double x){ return sqrt(x); }, [](double x){ return 1.0; }));
    check();

    BOOST_TEST_MESSAGE("--- create_message: alice");
    BOOST_CHECK_EQUAL(success(), create_message(N(alice), "permlink"));
    check();
    BOOST_CHECK_EQUAL(success(), setrules({"x", bignum}, {"sqrt(x)", bignum}, {"1", bignum}, 2500,
        [](double x){ return x; }, [](double x){ return sqrt(x); }, [](double x){ return 1.0; }));
    check();
    BOOST_TEST_MESSAGE("--- add_funds_to_forum");
    BOOST_CHECK_EQUAL(success(), add_funds_to_forum(50000));
    check();
    BOOST_TEST_MESSAGE("--- create_message: bob");
    BOOST_CHECK_EQUAL(success(), create_message(N(bob), "permlink1"));
    check();
    BOOST_TEST_MESSAGE("--- create_message: alice (1)");
    BOOST_CHECK_EQUAL(success(), create_message(N(alice), "permlink1"));
    check();
    BOOST_TEST_MESSAGE("--- alice1 voted for alice");
    BOOST_CHECK_EQUAL(success(), addvote(N(alice1), N(alice), "permlink", 100));
    check();
    BOOST_TEST_MESSAGE("--- bob1 voted (10%) for bob");
    BOOST_CHECK_EQUAL(success(), addvote(N(bob1), N(bob), "permlink1", 100));
    check();
    BOOST_TEST_MESSAGE("--- bob2 voted (100%) for bob");
    BOOST_CHECK_EQUAL(success(), addvote(N(bob2), N(bob), "permlink1", 10000));
    check();
    BOOST_TEST_MESSAGE("--- alice2 voted (100%) for alice (1)");
    BOOST_CHECK_EQUAL(success(), addvote(N(alice2), N(alice), "permlink1", 10000));
    check();
    BOOST_TEST_MESSAGE("--- alice3 flagged (80%) against bob");
    BOOST_CHECK_EQUAL(success(), addvote(N(alice3), N(bob), "permlink1", -8000));
    check();
    BOOST_TEST_MESSAGE("--- add_funds_to_forum");
    BOOST_CHECK_EQUAL(success(), add_funds_to_forum(100000));
    check();
    BOOST_TEST_MESSAGE("--- waiting");
    run(seconds(100));
    check();
    BOOST_TEST_MESSAGE("--- create_message: why");
    BOOST_CHECK_EQUAL(success(), create_message(N(why), "why not", N(), "", {{N(alice5), 5000}, {N(bob5), 2500}}));
    check();
    BOOST_TEST_MESSAGE("--- add_funds_to_forum");
    BOOST_CHECK_EQUAL(success(), add_funds_to_forum(100000));
    check();
    BOOST_TEST_MESSAGE("--- why voted (100%) for why");
    BOOST_CHECK_EQUAL(success(), addvote(N(why), N(why), "why not", 10000));
    check();
    run(seconds(100));
    BOOST_TEST_MESSAGE("--- rewards");
    check();
    show();

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(timepenalty_test, reward_calcs_tester) try {
    BOOST_TEST_MESSAGE("Simple timepenalty test");
    auto bignum = 500000000000;
    init(bignum, 500000);
    step();

    BOOST_CHECK_EQUAL(success(), setrules({"x", bignum}, {"x", bignum}, {"x/50", 50}, 2500,
        [](double x){ return x; }, [](double x){ return x; }, [](double x){ return x / 50.0; }));
    check();
    BOOST_TEST_MESSAGE("--- add_funds_to_forum");
    BOOST_CHECK_EQUAL(success(), add_funds_to_forum(50000));
    check();
    BOOST_TEST_MESSAGE("--- create_message: bob");
    BOOST_CHECK_EQUAL(success(), create_message(N(bob), "permlink"));
    check();
    BOOST_TEST_MESSAGE("--- voting");
    for (size_t i = 0; i < 6; i++) {
        BOOST_CHECK_EQUAL(success(), addvote(_users[i], N(bob), "permlink", 10000));
        check();
        run(seconds(2));
    }
    run(seconds(100));
    BOOST_TEST_MESSAGE("--- rewards");
    check();
    show();
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(limits_test, reward_calcs_tester) try {
    BOOST_TEST_MESSAGE("Simple limits test");
    auto bignum = 500000000000;
    init(bignum, 500000);

    BOOST_CHECK_EQUAL(success(), setrules({"x", bignum}, {"sqrt(x)", bignum}, {"x / 1800", 1800},
    0, //curatorsprop
    [](double x){ return x; }, [](double x){ return sqrt(x); }, [](double x){ return x / 1800.0; },
    {
        .restorers = {"sqrt(v / 500000) * (t / 150)", "t / 250"},
        .limitedacts = {
            {.chargenum = 0, .restorernum = 0,                 .cutoffval = 20000, .chargeprice = 9900}, //POST
            {.chargenum = 0, .restorernum = 0,                 .cutoffval = 30000, .chargeprice = 1000}, //COMMENT
            {.chargenum = 1, .restorernum = 1,                 .cutoffval = 10000, .chargeprice = 1000}, //VOTE
            {.chargenum = 0, .restorernum = disabled_restorer, .cutoffval = 10000, .chargeprice = 0}},   //POST BW
        .vestingprices = {150000, -1},
        .minvestings = {300000, 100000, 100000}
    },
    {
        [](double p, double v, double t){ return sqrt(v / 500000.0) * (t / 150.0); },
        [](double p, double v, double t){ return t / 250.0; }
    }));

    BOOST_TEST_MESSAGE("--- add_funds_to_forum");
    BOOST_CHECK_EQUAL(success(), add_funds_to_forum(100000));
    check();

    BOOST_CHECK_EQUAL(success(), create_message(N(bob), "permlink"));
    BOOST_CHECK_EQUAL(success(), create_message(N(bob), "permlink1"));
    BOOST_CHECK_EQUAL("assertion failure with message: publication::apply_limits: can't post, not enough power",
        create_message(N(bob), "permlink2"));
    BOOST_TEST_MESSAGE("--- comments");
    for (size_t i = 0; i < 10; i++)
        BOOST_CHECK_EQUAL(success(), create_message(N(bob), "comment" + std::to_string(i), N(bob), "permlink"));
    BOOST_CHECK_EQUAL("assertion failure with message: publication::apply_limits: can't post, not enough power",
        create_message(N(bob), "oops", N(bob), "permlink"));
    BOOST_CHECK_EQUAL("assertion failure with message: publication::apply_limits: can't post, not enough power",
        create_message(N(bob), "oops", N(bob), "permlink"));
    BOOST_CHECK_EQUAL(success(), create_message(N(bob), "I can pay for posting", N(), "", {}, 5000, true));
    BOOST_CHECK_EQUAL(
        "assertion failure with message: publication::apply_limits: can't post, not enough power, vesting payment is disabled",
        create_message(N(bob), "only if it is not a comment", N(bob), "permlink", {}, 5000, true));
    BOOST_CHECK_EQUAL(success(), addvote(N(alice), N(bob), "I can pay for posting", 10000));
    BOOST_CHECK_EQUAL(success(), addvote(N(bob), N(bob), "I can pay for posting", 10000)); //He can also vote
    BOOST_CHECK_EQUAL(success(), create_message(N(bob1), "permlink"));
    BOOST_TEST_MESSAGE("--- waiting");
    run(seconds(170));
    check();
    run(seconds(170));
    check();
    BOOST_CHECK_EQUAL(
        "assertion failure with message: publication::apply_limits: can't post, not enough power",
        create_message(N(bob), ":("));
    run(seconds(20));
    BOOST_CHECK_EQUAL(success(), create_message(N(bob), ":)"));
    check();
    show();
} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
