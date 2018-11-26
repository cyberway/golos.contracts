#pragma once
#include "test_api_helper.hpp"

namespace eosio { namespace testing {
using namespace fixed_point_utils;
struct golos_charge_api: base_contract_api {
    golos_charge_api(golos_tester* tester, name code, symbol sym)
    :   base_contract_api(tester, code)
    ,   _symbol(sym) {}
    symbol _symbol;

    action_result set_restorer(name issuer, unsigned char suffix, std::string func_str,
        uint64_t max_prev, uint64_t max_vesting, uint64_t max_elapsed) {
        return push(N(setrestorer), issuer, args()
            ("token_name", static_cast<uint64_t>(_symbol.to_symbol_code()))
            ("suffix", suffix)
            ("func_str", func_str)
            ("max_prev", max_prev)
            ("max_vesting", max_vesting)
            ("max_elapsed", max_elapsed)
        );
    }

    action_result use(name issuer, name user, unsigned char suffix, uint64_t price, uint64_t cutoff) {
        return push(N(use), issuer, args()
            ("user", user)
            ("token_name", static_cast<uint64_t>(_symbol.to_symbol_code()))
            ("suffix", suffix)
            ("price", price)
            ("cutoff", cutoff)
        );
    }
};


}} // eosio::testing
