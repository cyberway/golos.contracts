#pragma once
#include <eosio/chain/types.hpp>
#include <boost/test/unit_test.hpp>
#define UNIT_TEST_ENV
#include "../golos.publication/types.h"
#include "../golos.publication/config.hpp"
#include <string>
#include <vector>
#include <list>
#include <map>

using namespace eosio::chain;

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


inline double get_prop(int64_t arg) {
    return static_cast<double>(arg) / static_cast<double>(golos::config::_100percent);
}

struct message_key {
    account_name author;
    uint64_t id;
    bool operator ==(const message_key& rhs) const {
        return author == rhs.author && id == rhs.id;
    }
};


struct aprox_val_t {
    double val;
    double delta;
    bool operator ==(const aprox_val_t& rhs) const {
        BOOST_REQUIRE_MESSAGE((rhs.delta >= 0.0 && delta >= 0.0) || (rhs.delta <= 0.0 && delta <= 0.0),
            "aprox_val_t operator ==(): wrong comparison mode");
        if ((rhs.val > 0.0 && val < 0.0) || (rhs.val < 0.0 && val > 0.0))
            return false;
        double d = std::min(double(std::abs(rhs.delta)), std::abs(double(delta)));
        if (delta < 0.0) {
            double a = std::min(std::abs(rhs.val), std::abs(val));
            double b = std::max(std::abs(rhs.val), std::abs(val));
            return b < 1.e-20 || (a / b) > (1.0 - d);
        } else {
            return (val - d) <= rhs.val && rhs.val <= (val + d);
        }
    }

    aprox_val_t& operator = (const double& arg) { val = arg; return *this; }
    aprox_val_t& operator +=(const double& arg) { val += arg; return *this; }
};

inline std::ostream& operator<< (std::ostream& os, const aprox_val_t& rhs) {
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
        auto prefix = get_pool_str(id);
        operator[](prefix + "msgs") = { data.msgs, delta.pool.msgs };
        operator[](prefix + "funds") = { data.funds, delta.pool.funds };
        operator[](prefix + "rshares") = { data.rshares, delta.pool.rshares };
        operator[](prefix + "rsharesfn") = { data.rsharesfn, delta.pool.rsharesfn };
    }
    void set_balance(account_name acc, const balance_data data = {}) {
        auto prefix = get_balance_str(acc);
        operator[](prefix + "tokenamount") = { data.tokenamount, delta.balance.tokenamount };
        operator[](prefix + "vestamount") = { data.vestamount, delta.balance.vestamount };
    }

    void set_balance_token(account_name acc, double val) {
        auto prefix = get_balance_str(acc);
        operator[](prefix + "tokenamount") = { val, delta.balance.tokenamount };
    }

    void set_balance_vesting(account_name acc, double val) {
        auto prefix = get_balance_str(acc);
        operator[](prefix + "vestamount") = { val, delta.balance.vestamount };
    }

    void set_message(const message_key& msg, const message_data& data = {}) {

        auto prefix = get_message_str(msg);
        operator[](prefix + "absshares") = { data.absshares, delta.message.absshares };
        operator[](prefix + "netshares") = { data.netshares, delta.message.netshares };
        operator[](prefix + "voteshares") = { data.voteshares, delta.message.voteshares };
        operator[](prefix + "sumcuratorsw") = { data.sumcuratorsw, delta.message.sumcuratorsw };
    }

    pool_data get_pool(uint64_t id) {
        auto prefix = get_pool_str(id);
        return {
            operator[](prefix + "msgs").val,
            operator[](prefix + "funds").val,
            operator[](prefix + "rshares").val,
            operator[](prefix + "rsharesfn").val
        };
    }

    void remove_same(const statemap& rhs) {
        for (auto itr_rhs = rhs.begin(); itr_rhs != rhs.end(); itr_rhs++) {
            auto itr = find(itr_rhs->first);
            if ((itr != end()) && (itr->second == itr_rhs->second))
                erase(itr);
        }
    }
};

inline std::ostream& operator<< (std::ostream& os, const statemap& rhs) {
    for (auto itr = rhs.begin(); itr != rhs.end(); ++itr)
        os << itr->first << " = " << itr->second << "\n";
    return os;
}


struct vote {
    account_name voter;
    double weight;
    double vesting;
    double created;         // time
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
    double created;         // time
    std::vector<beneficiary> beneficiaries;
    double reward_weight;

    double get_rshares_sum() const {
        double ret = 0.0;
        for (auto& v : votes)
            ret += v.weight * v.vesting;
        return ret;
    };
    message(message_key k, double tokenprop_, double created_, const std::vector<beneficiary>& beneficiaries_, double reward_weight_) :
        key(k), tokenprop(tokenprop_), created(created_), beneficiaries(beneficiaries_), reward_weight(reward_weight_) {};
};

class cutted_func {
    std::function<double(double)> _fn;
    double _maxarg;
public:
    cutted_func(std::function<double(double)>&& f, double maxarg) : _fn(std::move(f)), _maxarg(maxarg) {};
    double operator()(double arg)const {return _fn(std::min(std::max(0.0, arg), _maxarg));};
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

    const limitedact& get_limited_act(kind_t kind) const {
        return limitedacts.at(static_cast<size_t>(kind));
    }

    const func_t* get_restorer(kind_t kind) const {
        uint8_t num = get_limited_act(kind).restorernum;
        return num == disabled_restorer ? nullptr : &(restorers.at(num));
    }

    double get_vesting_price(kind_t kind) const {
        auto k = static_cast<size_t>(kind);
        return k < vestingprices.size() ? vestingprices[k] : -1.0;
    }

    double get_min_vesting_for(kind_t kind) const {
        return minvestings.at(static_cast<size_t>(kind));
    }

    size_t get_charges_num() const {
        size_t ret = 0;
        for (auto& act : limitedacts)
            ret = std::max(ret, static_cast<size_t>(act.chargenum) + 1);
        return ret;
    }

    bool check() const {
        if (vestingprices.size() >= 3 || minvestings.size() != 3)
            return false;
        for (auto& act : limitedacts)
            if (!(act.restorernum < restorers.size()) || act.restorernum == disabled_restorer)
                return false;
        return true;
    }

    void update_charge(chargeinfo& chg, const func_t* restorer, uint64_t cur_time, double vesting, double price, double w = 1.0) const {
        using namespace golos::config;    // for limit_restorer_domain
        if (restorer != nullptr) {
            auto prev = std::min(chg.data, static_cast<double>(limit_restorer_domain::max_prev));
            auto v_corr = std::min(vesting, static_cast<double>(limit_restorer_domain::max_vesting));
            auto elapsed_seconds = std::min(static_cast<double>((cur_time - chg.lastupdate) / fc::seconds(1).count()),
                static_cast<double>(limit_restorer_domain::max_elapsed_seconds));
            auto restored = (*restorer)(prev, v_corr, elapsed_seconds);
            if (restored > prev)
                chg.data = 0.0;
            else
                chg.data = std::min(chg.data - restored, static_cast<double>(limit_restorer_domain::max_res));
        }
        if (price > 0.0) {
            auto added = std::max(price * w, static_cast<double>(std::numeric_limits<fixed_point_utils::fixp_t>::min()));
            chg.data = std::min(chg.data + added, static_cast<double>(limit_restorer_domain::max_res));
        }
        chg.lastupdate = cur_time;
    }
    // charges are not const here
    double calc_power(kind_t kind, usercharges& charges, uint64_t cur_time, double vesting, double w) const {
        auto& lim_act = get_limited_act(kind);
        auto restorer = get_restorer(kind);
        auto& chg = charges[lim_act.chargenum];
        update_charge(chg, restorer, cur_time, vesting, get_prop(lim_act.chargeprice), w);
        return (chg.data > get_prop(lim_act.cutoffval)) ? get_prop(lim_act.cutoffval) / chg.data : 1.0;
    }

    double apply(kind_t kind, usercharges& charges, double& vesting_ref, uint64_t cur_time, double w) const {
        if (kind == limits::VOTE && w != 0) {
            if (w * vesting_ref < get_min_vesting_for(kind))
                return -1.0;
        }
        else if (kind != limits::VOTE && vesting_ref < get_min_vesting_for(kind))
            return -1.0;

        auto charges_prev = charges;
        if (charges.empty())
            charges.resize(get_charges_num(), {cur_time, 0});
        auto power = calc_power(kind, charges, cur_time, vesting_ref, (kind == limits::VOTE) ? w : 1.0);
        if (power != 1.0)
            charges = charges_prev;

        if (kind == limits::VOTE)
            return power == 1.0 ? 1.0 : -1.0;

        if (power != 1.0 && w == 0.0)
            return -1.0;

        if (power != 1.0) {//=> w != 0
            auto price = get_vesting_price(kind);
            if (price <= 0 || price > vesting_ref)
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

    rewardpool(uint64_t id_, rewardrules&& rules_, limits&& lims_)
    :   id(id_), funds(0.0), rules(std::move(rules_)), lims(std::move(lims_)) {};

    double get_rshares_sum()const {
        double ret = 0.0;
        for (auto& m : messages)
            ret += m.get_rshares_sum();
        return ret;
    }

    //yup, it's dumb. but we are testing here, we don't need performance, we prefer straightforward logic
    double get_rsharesfn_sum() const {
        double ret = 0.0;
        for (auto& m : messages)
            ret += rules.mainfunc(m.get_rshares_sum());
        return ret;
    }

    double get_ratio() const {
        BOOST_REQUIRE_MESSAGE(funds >= 0.0, "rewardpool: wrong payment mode");
        auto r = get_rshares_sum();
        return r > 0 ? funds / r : std::numeric_limits<double>::max();
    }
};

struct state {
    std::map<account_name, balance_data> balances;
    std::vector<rewardpool> pools;
    void clear() { balances.clear(); pools.clear(); };
};
