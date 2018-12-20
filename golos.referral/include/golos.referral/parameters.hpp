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

   void validate() const override {
        eosio_assert(max_expire >= 0, "max_breakout < 0");
   }
};
using expire_param = param_wrapper<expire_parametrs,1>;

struct persent_parametrs : parameter {
   uint32_t max_perсent;

   void validate() const override {
        eosio_assert(max_perсent <= 10000, "max_perсent > 100.00%");
   }
};
using persent_param = param_wrapper<persent_parametrs,1>;

using referral_params = std::variant<breakout_param, expire_param, persent_param>;

struct [[eosio::table]] referral_state {
    breakout_param breakout_params;
    expire_param   expire_params;
    persent_param  persent_params;

    static constexpr int params_count = 3;
};
using referral_params_singleton = eosio::singleton<"refparams"_n, referral_state>;

} // golos
