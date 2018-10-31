#pragma once
#include "test_api_helper.hpp"

namespace eosio { namespace testing {


struct eosio_token_api: base_contract_api {
    using base_contract_api::base_contract_api;
    symbol _symbol;

    //// token actions
    action_result create(account_name issuer, asset maximum_supply) {
        return push(N(create), _code, args()
            ("issuer", issuer)
            ("maximum_supply", maximum_supply)
        );
    }

    action_result issue(account_name issuer, account_name to, asset quantity, string memo) {
        return push(N(issue), issuer, args()
            ("to", to)
            ("quantity", quantity)
            ("memo", memo)
        );
    }

    action_result transfer(account_name from, account_name to, asset quantity, string memo) {
        return push(N(transfer), from, args()
            ("from", from)
            ("to", to)
            ("quantity", quantity)
            ("memo", memo)
        );
    }

    //// token tables
    variant get_stats() {
        auto sname = _symbol.to_symbol_code().value;
        auto v = get_struct(sname, N(stat), sname, "currency_stats");
        if (v.is_object()) {
            auto o = mvo(v);
            o["supply"] = o["supply"].as<asset>().to_string();
            o["max_supply"] = o["max_supply"].as<asset>().to_string();
            v = o;
        }
        return v;
    }

    variant get_account(account_name acc) {
        auto v = get_struct(acc, N(accounts), _symbol.to_symbol_code().value, "account");
        if (v.is_object()) {
            auto o = mvo(v);
            o["balance"] = o["balance"].as<asset>().to_string();
            v = o;
        }
        return v;
    }

    //// helpers

};


}} // eosio::testing
