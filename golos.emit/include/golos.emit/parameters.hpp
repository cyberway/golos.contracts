#pragma once
#include <common/parameter.hpp>
#include <common/config.hpp>
#include <eosio/singleton.hpp>
#include <eosio/symbol.hpp>

namespace golos {


using namespace eosio;

struct reward_pool {
    name name;
    uint16_t percent;
};
// TODO: use pfr
bool operator==(const reward_pool& l, const reward_pool& r) {
    return l.name == r.name && l.percent == r.percent;
}
bool operator<(const reward_pool& l, const reward_pool& r) {
    return std::tie(l.name, l.percent) < std::tie(r.name, r.percent);
}

struct reward_pools_t: parameter {
    std::vector<reward_pool> pools;

    void validate() const override {
        auto n_pools = pools.size();
        eosio::check(n_pools > 0, "at least 1 pool must be set");
        uint32_t total = 0;
        name prev_name = name();
        bool first = true;
        bool have0 = false;
        for (const auto& p: pools) {
            eosio::check(is_account(p.name), "pool account must exist");
            if (!first) {
                eosio::check(p.name > prev_name,
                    p.name < prev_name ? "pools must be sorted by name" : "pool with same name already exists");
            }
            if (p.percent) {
                if (first)
                    eosio::check(n_pools > 1, "the only pool must have percent value of 0");
                eosio::check(p.percent < config::_100percent, "percent must be less than 100%");
                total += p.percent;
                eosio::check(total < config::_100percent, "sum of pools percents must be < 100%");
            } else {
                eosio::check(!have0, "only 1 pool allowed to have auto-percent (0 value)");
                have0 = true;
            }
            first = false;
            prev_name = p.name;
        }
        eosio::check(have0, "1 of pools must have 0% value (remainig to 100%)");
    }
};
using reward_pools = param_wrapper<reward_pools_t,1>;

struct inflation_rate_t: parameter {
    uint16_t start;         // INFLATION_RATE_START_PERCENT
    uint16_t stop;          // INFLATION_RATE_STOP_PERCENT
    uint32_t narrowing;     // INFLATION_NARROWING_PERIOD

    void validate() const override {
        eosio::check(start >= stop, "inflation_rate: start can not be less than stop");
        eosio::check(start != stop || narrowing == 0, "inflation_rate: if start == stop then narrowing must be 0");
    }
};
using inflation_rate = param_wrapper<inflation_rate_t,3>;

struct emit_token_t: parameter {
    symbol symbol;
};
using emit_token = param_wrapper<emit_token_t,1>;

struct emit_interval_t: parameter {
    uint16_t value;
    
    void validate() const override {
        eosio::check(value > 0, "interval must be positive");
    }
};
using emit_interval = param_wrapper<emit_interval_t,1>;

struct bwprovider_t: parameter {
    name actor;
    name permission;

    void validate() const override {
        eosio::check((actor != name()) == (permission != name()), "actor and permission must be set together");
        // check that contract can use cyber:providebw done in setparams
        // (we need know contract account to make this check)
    }
};
using bwprovider = param_wrapper<bwprovider_t,2>;

using emit_param = std::variant<inflation_rate, reward_pools, emit_token, emit_interval, bwprovider>;

struct emit_state {
    inflation_rate infrate;
    reward_pools pools;
    emit_token token;
    emit_interval interval; 
    bwprovider bwprovider;

    static constexpr int params_count = 5;
};
using emit_params_singleton [[using eosio: order("id","asc"), contract("golos.emit")]] = eosio::singleton<"emitparams"_n, emit_state>;


} // golos
