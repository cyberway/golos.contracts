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

struct reward_pools: parameter {
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
using reward_pools_param = param_wrapper<reward_pools,1>;

struct inflation_rate: parameter {
    uint16_t start;         // INFLATION_RATE_START_PERCENT
    uint16_t stop;          // INFLATION_RATE_STOP_PERCENT
    uint32_t narrowing;     // INFLATION_NARROWING_PERIOD

    void validate() const override {
        eosio::check(start >= stop, "inflation_rate: start can not be less than stop");
        eosio::check(start != stop || narrowing == 0, "inflation_rate: if start == stop then narrowing must be 0");
    }
};
using infrate_params = param_wrapper<inflation_rate,3>;

struct emit_token: parameter {
    symbol symbol;
};
using emit_token_params = param_wrapper<emit_token,1>;

struct emit_interval: parameter {
    uint16_t value;
    
    void validate() const override {
        eosio::check(value > 0, "interval must be positive");
    }
};
using emit_interval_param = param_wrapper<emit_interval,1>;

struct bwprovider: parameter {
    permission_level provider;

    void validate() const override {
        eosio::check((provider.actor != name()) == (provider.permission != name()), "actor and permission must be set together");
        // check that contract can use cyber:providebw done in setparams
        // (we need know contract account to make this check)
    }
};
using bwprovider_param = param_wrapper<bwprovider,1>;

using emit_param = std::variant<infrate_params, reward_pools_param, emit_token_params, emit_interval_param, bwprovider_param>;

struct [[eosio::table]] emit_state {
    infrate_params infrate;
    reward_pools_param pools;
    emit_token_params token;
    emit_interval_param interval; 
    bwprovider_param bwprovider;

    static constexpr int params_count = 5;
};
using emit_params_singleton = eosio::singleton<"emitparams"_n, emit_state>;


} // golos
