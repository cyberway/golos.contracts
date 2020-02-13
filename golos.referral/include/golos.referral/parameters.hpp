#pragma once
#include <common/parameter.hpp>
#include <eosio/asset.hpp>
#include <eosio/singleton.hpp>
#include <common/config.hpp>
#include <common/parameter_ops.hpp>


namespace golos {
using namespace eosio;

struct breakout_parametrs_t : parameter {
    asset min_breakout;
    asset max_breakout;

    void validate() const override {
        eosio::check(min_breakout <= max_breakout, "min_breakout > max_breakout");
        eosio::check(min_breakout.amount >= 0, "min_breakout < 0");
        // max_breakout is already non-negative here
    }

    static bool not_equal(const breakout_parametrs_t& obj, const breakout_parametrs_t& other) {
        return
            (obj.max_breakout.symbol != other.max_breakout.symbol) ||
            (obj.min_breakout.symbol != other.min_breakout.symbol) ||
            (obj.min_breakout.amount != other.min_breakout.amount) ||
            (obj.min_breakout.amount != other.min_breakout.amount);
    }
};
using breakout_parametrs = param_wrapper<breakout_parametrs_t,2>;

struct expire_parametrs_t : parameter {
    uint64_t max_expire;
};
using expire_parametrs = param_wrapper<expire_parametrs_t,1>;

struct percent_parametrs_t : parameter {
    uint16_t max_percent;

    void validate() const override {
        eosio::check(max_percent <= 10000, "max_percent > 100.00%");
    }
};
using percent_parametrs = param_wrapper<percent_parametrs_t,1>;

using referral_params = std::variant<breakout_parametrs, expire_parametrs, percent_parametrs>;

struct referral_state {
    breakout_parametrs breakout_params;
    expire_parametrs   expire_params;
    percent_parametrs  percent_params;

    static constexpr int params_count = 3;
};
using referral_params_singleton [[using eosio: order("id","asc"), contract("golos.referral")]] = eosio::singleton<"refparams"_n, referral_state>;

} // golos
