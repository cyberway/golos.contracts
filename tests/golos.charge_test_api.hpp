#pragma once
#include "test_api_helper.hpp"

#define CHARGE_DISABLE_VESTING
#define CHARGE_DISABLE_STORABLE

namespace eosio { namespace testing {
struct golos_charge_api: base_contract_api {
    golos_charge_api(golos_tester* tester, name code, symbol sym)
    :   base_contract_api(tester, code)
    ,   _symbol(sym) {}
    symbol _symbol;

    void initialize_contract() {
        _tester->install_contract(_code, contracts::charge_wasm(), contracts::charge_abi());
    }

    // actions
    action_result set_restorer(name issuer, uint8_t charge_id, std::string func_str,
        int64_t max_prev, int64_t max_vesting, int64_t max_elapsed) {
        return push(N(setrestorer), issuer, args()
            ("token_code", _symbol.to_symbol_code())
            ("charge_id", charge_id)
            ("func_str", func_str)
            ("max_prev", max_prev)
            ("max_vesting", max_vesting)
            ("max_elapsed", max_elapsed)
        );
    }

    action_result use(name issuer, name user, uint8_t charge_id, int64_t price, int64_t cutoff, int64_t vesting_price = 0) {
        if (vesting_price != 0) {
            BOOST_CHECK(_tester->has_code_authority(issuer, cfg::invoice_name, _code));
            BOOST_CHECK(_tester->has_link_authority(issuer, cfg::invoice_name, cfg::vesting_name, N(retire)));
        }

        return push(N(use), issuer, args()
            ("user", user)
            ("token_code", _symbol.to_symbol_code())
            ("charge_id", charge_id)
            ("price", price)
            ("cutoff", cutoff)
            ("vesting_price", vesting_price)
        );
    }

    action_result use_and_store(name issuer, name user, uint8_t charge_id, int64_t stamp_id, int64_t price) {
        return push(N(useandstore), issuer, args()
            ("user", user)
            ("token_code", _symbol.to_symbol_code())
            ("charge_id", charge_id)
            ("stamp_id", stamp_id)
            ("price", price)
        );
    }

    action_result remove_stored(name issuer, name user, uint8_t charge_id, int64_t stamp_id) {
        return push(N(removestored), issuer, args()
            ("user", user)
            ("token_code", _symbol.to_symbol_code())
            ("charge_id", charge_id)
            ("stamp_id", stamp_id)
        );
    }

    // tables
    variant get_balance(name acc, uint8_t charge_id) {
        // base_api_helper knows code
        return get_struct(acc, N(balances), _symbol.to_symbol_code().value << 8 | charge_id, "balance");
    }
};


}} // eosio::testing
