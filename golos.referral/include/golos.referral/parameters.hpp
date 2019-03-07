#pragma once
#include <common/parameter.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/singleton.hpp>
#include <common/config.hpp>
#include <common/parameter_ops.hpp>


namespace golos {
using namespace eosio;

struct breakout_parametrs : parameter {
   asset min_breakout;
   asset max_breakout;

   void validate() const override {
        eosio_assert(min_breakout <= max_breakout, "min_breakout > max_breakout");
        eosio_assert(min_breakout.amount >= 0, "min_breakout < 0");
        eosio_assert(max_breakout.amount >= 0, "max_breakout < 0");
   }
};
using breakout_param = param_wrapper<breakout_parametrs,2>;

struct expire_parametrs : parameter {
   uint64_t max_expire;

   void validate() const override {}
};
using expire_param = param_wrapper<expire_parametrs,1>;

struct percent_parametrs : parameter {
   uint32_t max_percent;

   void validate() const override {
        eosio_assert(max_percent <= 10000, "max_percent > 100.00%");
   }
};
using percent_param = param_wrapper<percent_parametrs,1>;

struct delay_parametrs : parameter {
   uint32_t delay_clear_old_ref;

   void validate() const override {}
};
using delay_param = param_wrapper<delay_parametrs,1>;

using referral_params = std::variant<breakout_param, expire_param, percent_param, delay_param>;

struct [[eosio::table]] referral_state {
    breakout_param breakout_params;
    expire_param   expire_params;
    percent_param  percent_params;
    delay_param    delay_params;

    static constexpr int params_count = 4;
};
using referral_params_singleton = eosio::singleton<"refparams"_n, referral_state>;

} // golos
