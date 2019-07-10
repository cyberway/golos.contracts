#pragma once
#include "test_api_helper.hpp"
#include "contracts.hpp"
#include "common/config.hpp"

namespace eosio { namespace testing {

namespace cfg = golos::config;

struct golos_emit_api: base_contract_api {
    using base_contract_api::base_contract_api;

    void initialize_contract(name token, name issuer) {
        _tester->install_contract(_code, contracts::emit_wasm(), contracts::emit_abi());

        _tester->set_authority(_code, cfg::code_name, create_code_authority({_code}), "active");
        _tester->link_authority(_code, _code, cfg::code_name, N(emit));

        BOOST_CHECK(_tester->has_code_authority(issuer, cfg::issue_name, _code));
        _tester->link_authority(issuer, token, cfg::issue_name, N(issue));
        _tester->link_authority(issuer, token, cfg::issue_name, N(transfer));
    }

    //// emit actions
    action_result emit(account_name signer) {
        return push(N(emit), signer, args());
    }

    action_result start() {
        return start(_code);
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
        return get_struct(_code, N(state), N(state), "state");
    }

    variant get_params() const {
        return get_account_params(_code);
    }

    variant get_account_params(account_name acc) const {
        return base_contract_api::get_struct(acc, N(emitparams), N(emitparams), "params");
    }

    // helpers
    string pool_json(account_name pool, uint16_t percent) {
        return string("{'name':'") + name{pool}.to_string() + "','percent':" + std::to_string(percent) + "}";
    }
    string infrate_json(uint16_t start, uint16_t stop, uint32_t narrowing = 0) {
        return string("['inflation_rate',{'start':") + std::to_string(start) + ",'stop':" + std::to_string(stop)
            + ",'narrowing':" + std::to_string(narrowing) + "}]";
    }
    string token_symbol_json(symbol symbol) {
        return string("['emit_token',{'symbol':'" + std::to_string(symbol.decimals()) + "," + symbol.name() + "'}]");
    }

};


}} // eosio::testing
