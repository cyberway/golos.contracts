#pragma once
#include <eosiolib/singleton.hpp>
#include <common/parameter.hpp>
#include <common/parameter_ops.hpp>

namespace golos {

    using namespace eosio;

    struct vesting_acc : parameter {
        name account;

        void validate() const override {
            eosio_assert(is_account(account), "Vesting account doesn't exist.");
        }
    };
    using vest_acc_param = param_wrapper<vesting_acc, 1>;


    using charge_params = std::variant<vest_acc_param>;

    struct [[eosio::table]] charge_state {
        vest_acc_param vesting_acc;

        static constexpr int params_count = 1;
    };
    using charge_params_singleton = eosio::singleton<"chrgparams"_n, charge_state>;

} //golos
