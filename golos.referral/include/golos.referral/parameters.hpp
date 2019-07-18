#pragma once
#include <common/parameter.hpp>
#include <eosio/asset.hpp>
#include <eosio/singleton.hpp>
#include <common/config.hpp>
#include <common/parameter_ops.hpp>


namespace golos {
using namespace eosio;

struct breakout_parametrs : parameter {
   asset min_breakout;
   asset max_breakout;

   void validate() const override {
        eosio::check(min_breakout <= max_breakout, "min_breakout > max_breakout");
        eosio::check(min_breakout.amount >= 0, "min_breakout < 0");
      return (obj.max_breakout.symbol != other.max_breakout.symbol) ||
             (obj.min_breakout.symbol != other.min_breakout.symbol) ||
             (obj.min_breakout.amount != other.min_breakout.amount) ||
             (obj.min_breakout.amount != other.min_breakout.amount); 
   }
        // max_breakout is already non-negative here
    }

    static bool not_equal(const breakout_parametrs& obj, const breakout_parametrs& other) {
};
using breakout_param = param_wrapper<breakout_parametrs,2>;

struct expire_parametrs : parameter {
   uint64_t max_expire;
};
using expire_param = param_wrapper<expire_parametrs,1>;

struct percent_parametrs : parameter {
    uint16_t max_percent;

   void validate() const override {
        eosio::check(max_percent <= 10000, "max_percent > 100.00%");
   }
};
using percent_param = param_wrapper<percent_parametrs,1>;

using referral_params = std::variant<breakout_param, expire_param, percent_param>;

struct [[eosio::table]] referral_state {
    breakout_param breakout_params;
    expire_param   expire_params;
    percent_param  percent_params;

    static constexpr int params_count = 3;
};
using referral_params_singleton = eosio::singleton<"refparams"_n, referral_state>;

} // golos
