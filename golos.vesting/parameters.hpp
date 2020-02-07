#pragma once
#include <common/parameter.hpp>
#include <eosio/singleton.hpp>
#include <common/config.hpp>
#include <common/parameter_ops.hpp>


namespace golos {


using namespace eosio;

struct vesting_withdraw : parameter {
    uint8_t intervals;
    uint32_t interval_seconds;

    void validate() const override {
        eosio::check(intervals > 0, "intervals <= 0");
        eosio::check(interval_seconds > 0, "interval_seconds <= 0");
    }
};
using vesting_withdraw_param = param_wrapper<vesting_withdraw,2>;


struct vesting_min_amount : parameter {
    uint64_t min_amount;    // withdraw
};
using vesting_min_amount_param = param_wrapper<vesting_min_amount,1>;


struct vesting_delegation : parameter {
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
using delegation_param = param_wrapper<vesting_delegation,5>;

struct bwprovider: parameter {
    permission_level provider;

    void validate() const override {
        eosio::check((provider.actor != name()) == (provider.permission != name()), "actor and permission must be set together");
        // check that contract can use cyber:providebw done in setparams
        // (we need know contract account to make this check)
    }
};
using bwprovider_param = param_wrapper<bwprovider,1>;

using vesting_param = std::variant<param_wrapper<vesting_withdraw,2>, param_wrapper<vesting_min_amount,1>, param_wrapper<vesting_delegation,5>, param_wrapper<bwprovider,1>>;

struct [[eosio::table]] vesting_state {
    vesting_withdraw_param withdraw;
    vesting_min_amount_param min_amount;
    delegation_param delegation;
    bwprovider_param bwprovider;

    static constexpr int params_count = 4;
};
using vesting_params_singleton = eosio::singleton<"vestparams"_n, vesting_state>;

} // golos
