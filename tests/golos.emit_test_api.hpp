#pragma once
#include "test_api_helper.hpp"

namespace eosio { namespace testing {


struct golos_emit_api: domain_contract_api<symbol> {
    using domain_contract_api::domain_contract_api;

    //// emit actions
    action_result emit(account_name signer) {
        return push(N(emit), signer, args());
    }

    action_result start(account_name signer) {
        return push(N(start), signer, args());
    }
    action_result start(account_name owner, vector<account_name> signers) {
        return push_msig(N(start), {{owner, config::active_name}}, signers, args());
    }

    action_result stop(account_name signer) {
        return push(N(stop), signer, args());
    }
    action_result stop(account_name owner, vector<account_name> signers) {
        return push_msig(N(stop), {{owner, config::active_name}}, signers, args());
    }

    action_result set_params(std::string json_params) {
        return push(N(setparams), _code, args()
            ("params", json_str_to_obj(json_params)));
    }

    //// emit tables
    variant get_state() const {
        return get_struct(N(state), N(state), "state");
    }

    variant get_params() const {
        return get_account_params(_code);
    }

    variant get_account_params(account_name acc) const {
        return base_contract_api::get_struct(acc, N(params), N(params), "params");
    }

};


}} // eosio::testing
