#pragma once
#include <common/parameter.hpp>
#include <eosiolib/singleton.hpp>
#include <common/config.hpp>
#include <common/parameter_ops.hpp>


namespace golos {


using namespace eosio;

struct vesting_withdraw : parameter {
    uint32_t intervals;
    uint32_t interval_seconds;

    void validate() const override {
        eosio_assert(intervals > 0, "intervals <= 0");
        eosio_assert(interval_seconds > 0, "interval_seconds <= 0");
    }
};
using vesting_withdraw_param = param_wrapper<vesting_withdraw,2>;


struct vesting_min_amount : parameter {
    uint64_t min_amount;
};
using vesting_min_amount_param = param_wrapper<vesting_min_amount,1>;


struct delegation : parameter {
    uint64_t min_amount;
    uint64_t min_remainder;
    uint32_t min_time;
    uint16_t max_interest;
    uint32_t return_time;

    void validate() const override {
        eosio_assert(min_amount > 0, "delegation min_amount <= 0");
        eosio_assert(min_remainder > 0, "delegation min_remainder <= 0");
        eosio_assert(max_interest <= 10000, "delegation max_interest > 10000");
        eosio_assert(return_time > 0, "delegation return_time <= 0");
    }
};
using delegation_param = param_wrapper<delegation,5>;

using vesting_params = std::variant<vesting_withdraw_param, vesting_min_amount_param, delegation_param>;

struct [[eosio::table]] vesting_state {
    vesting_withdraw_param vesting_withdraw_params;
    vesting_min_amount_param amount_params;
    delegation_param delegation_params;

    static constexpr int params_count = 3;
};
using vesting_params_singleton = eosio::singleton<"vstngparams"_n, vesting_state>;

} // golos
