#pragma once
#include <common/parameter.hpp>
#include <common/config.hpp>
#include <eosio/singleton.hpp>

namespace golos { namespace param {


using namespace eosio;

struct ctrl_token_t: immutable_parameter {
    symbol_code code;

    void validate() const override {
        eosio::check(code.is_valid(), "invalid token");
    }
};

struct multisig_acc_t: parameter {
    name name;

    void validate() const override {
        eosio::check(is_account(name), "multisig account doesn't exists");
    }
};

// set to immutable for now coz it requires weights recalculation if decreasing `max` value
struct max_witness_votes_t: immutable_parameter {
    uint16_t max = 30;   // max witness votes

    void validate() const override {
        eosio::check(max > 0, "max witness votes can't be 0");
    }
};

struct max_witnesses_t: parameter {
    uint16_t max = 21;   // max witnesses

    void validate() const override {
        eosio::check(max > 0, "max witnesses can't be 0");
    }
};

struct multisig_perms_t: parameter {
    uint16_t super_majority = 0;    // 0 = auto (2/3+1)
    uint16_t majority = 0;          // 0 = auto (1/2+1)
    uint16_t minority = 0;          // 0 = auto (1/3+1)

    void validate() const override {
        // NOTE: can't compare with max_witnesses here (incl. auto-calculated values), do it in visitor
        eosio::check(!super_majority || !majority || super_majority >= majority, "super_majority must not be less than majority");
        eosio::check(!super_majority || !minority || super_majority >= minority, "super_majority must not be less than minority");
        eosio::check(!majority || !minority || majority >= minority, "majority must not be less than minority");
    }

    uint16_t super_majority_threshold(uint16_t top) const;
    uint16_t majority_threshold(uint16_t top) const;
    uint16_t minority_threshold(uint16_t top) const;
};

struct update_auth_t: parameter {
    uint32_t period = 30*60;

    void validate() const override {
        eosio::check(period > 0, "update auth period can't be 0");
    }
};

} // param

using ctrl_token = param_wrapper<param::ctrl_token_t,1>;
using multisig_acc = param_wrapper<param::multisig_acc_t,1>;
using max_witnesses = param_wrapper<param::max_witnesses_t,1>;
using multisig_perms = param_wrapper<param::multisig_perms_t,3>;
using max_witness_votes = param_wrapper<param::max_witness_votes_t,1>;
using update_auth = param_wrapper<param::update_auth_t,1>;

#define ctrl_param std::variant<ctrl_token, multisig_acc, max_witnesses, multisig_perms, max_witness_votes, update_auth>

struct ctrl_state {
    ctrl_token        token;
    multisig_acc      multisig;
    max_witnesses     witnesses;
    multisig_perms    msig_perms;
    max_witness_votes witness_votes;
    update_auth       update_auth_period;

    static constexpr int params_count = 6;
};
using ctrl_params_singleton [[using eosio: order("id","asc"), contract("golos.ctrl")]] = eosio::singleton<"ctrlparams"_n, ctrl_state>;


} // golos
