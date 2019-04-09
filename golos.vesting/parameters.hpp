#pragma once
#include <common/parameter.hpp>
#include <eosiolib/singleton.hpp>
#include <common/config.hpp>
#include <common/parameter_ops.hpp>


namespace golos {


using namespace eosio;

struct vesting_withdraw : parameter {
    uint32_t intervals;         // TODO: this value is uint8 inside withdraw_object, we should either limit it here, or change type here or in object #553
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


struct vesting_delegation : parameter {
    uint64_t min_amount;
    uint64_t min_remainder;
    uint32_t return_time;
    uint32_t min_time;
    uint16_t max_interest;  // TODO: should be moved to it's own parameter if we want to change it independently

    void validate() const override {
        eosio_assert(min_amount > 0, "delegation min_amount <= 0");
        eosio_assert(min_remainder > 0, "delegation min_remainder <= 0");
        eosio_assert(max_interest <= 10000, "delegation max_interest > 10000");
        eosio_assert(return_time > 0, "delegation return_time <= 0");
    }
};
using delegation_param = param_wrapper<vesting_delegation,5>;

using vesting_param = std::variant<vesting_withdraw_param, vesting_min_amount_param, delegation_param>;

struct [[eosio::table]] vesting_state {
    vesting_withdraw_param withdraw;
    vesting_min_amount_param min_amount;
    delegation_param delegation;

    static constexpr int params_count = 3;
};
using vesting_params_singleton = eosio::singleton<"vestparams"_n, vesting_state>;

} // golos
