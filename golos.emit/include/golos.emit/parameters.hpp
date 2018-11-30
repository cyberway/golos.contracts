#pragma once
#include <common/parameter.hpp>
#include <common/config.hpp>
#include <eosiolib/singleton.hpp>

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
        eosio_assert(n_pools > 0, "at least 1 pool must be set");
        uint32_t total = 0;
        name prev_name = name();
        bool first = true;
        bool have0 = false;
        for (const auto& p: pools) {
            eosio_assert(is_account(p.name), "pool account must exist");
            if (!first) {
                eosio_assert(p.name > prev_name,
                    p.name < prev_name ? "pools must be sorted by name" : "pool with same name already exists");
            }
            if (p.percent) {
                if (first)
                    eosio_assert(n_pools > 1, "the only pool must have percent value of 0");
                eosio_assert(p.percent < config::_100percent, "percent must be less than 100%");
                total += p.percent;
                eosio_assert(total < config::_100percent, "sum of pools percents must be < 100%");
            } else {
                eosio_assert(!have0, "only 1 pool allowed to have auto-percent (0 value)");
                have0 = true;
            }
            first = false;
            prev_name = p.name;
        }
        eosio_assert(have0, "1 of pools must have 0% value (remainig to 100%)");
    }
};
using reward_pools_param = param_wrapper<reward_pools,1>;

struct inflation_rate: parameter {
    uint16_t start;         // INFLATION_RATE_START_PERCENT
    uint16_t stop;          // INFLATION_RATE_STOP_PERCENT
    uint32_t narrowing;     // INFLATION_NARROWING_PERIOD

    void validate() const override {
        eosio_assert(start >= stop, "inflation_rate: start can not be less than stop");
        eosio_assert(start != stop || narrowing == 0, "inflation_rate: if start == stop then narrowing must be 0");
    }
};
using infrate_params = param_wrapper<inflation_rate,3>;

using emit_param = std::variant<infrate_params, reward_pools_param>;

struct [[eosio::table]] emit_state {
    infrate_params infrate;
    reward_pools_param pools;

    static constexpr int params_count = 2;
};
using emit_params_singleton = eosio::singleton<"emitparams"_n, emit_state>;


} // golos
