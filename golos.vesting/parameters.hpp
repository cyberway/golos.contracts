#pragma once
#include <common/parameter.hpp>
#include <eosio/singleton.hpp>
#include <common/config.hpp>
#include <common/parameter_ops.hpp>


namespace golos {


using namespace eosio;

struct vesting_withdraw_t : parameter {
    uint8_t intervals;
    uint32_t interval_seconds;

    void validate() const override {
        eosio::check(intervals > 0, "intervals <= 0");
        eosio::check(interval_seconds > 0, "interval_seconds <= 0");
    }
};
using vesting_withdraw = param_wrapper<vesting_withdraw_t,2>;


struct vesting_amount_t : parameter {
    uint64_t min_amount;    // withdraw
};
using vesting_amount = param_wrapper<vesting_amount_t,1>;


struct vesting_delegation_t : parameter {
    uint64_t min_amount;
    uint64_t min_remainder;
    uint32_t return_time;
    uint32_t min_time;
    uint32_t max_delegators;

    void validate() const override {
        eosio::check(min_amount > 0, "delegation min_amount <= 0");
        eosio::check(min_remainder > 0, "delegation min_remainder <= 0");
        eosio::check(return_time > 0, "delegation return_time <= 0");
    }
};
using vesting_delegation = param_wrapper<vesting_delegation_t,5>;

struct vesting_bwprovider_t: parameter {
    name actor;
    name permission;

    void validate() const override {
        eosio::check((actor != name()) == (permission != name()), "actor and permission must be set together");
        // check that contract can use cyber:providebw done in setparams
        // (we need know contract account to make this check)
    }
};
using vesting_bwprovider = param_wrapper<vesting_bwprovider_t,2>;

#define vesting_param std::variant<vesting_withdraw, vesting_amount, vesting_delegation, vesting_bwprovider>

struct vesting_state {
    vesting_withdraw withdraw;
    vesting_amount min_amount;
    vesting_delegation delegation;
    vesting_bwprovider bwprovider;

    static constexpr int params_count = 4;
};
using vesting_params_singleton [[using eosio: order("id","asc"), scope_type("symbol_code"), contract("golos.vesting")]] = eosio::singleton<"vestparams"_n, vesting_state>;

} // golos
