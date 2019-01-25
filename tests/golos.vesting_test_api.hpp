#pragma once
#include "test_api_helper.hpp"
#include "../common/config.hpp"

namespace eosio { namespace testing {


struct golos_vesting_api: base_contract_api {
    golos_vesting_api(golos_tester* tester, name code, symbol sym)
    :   base_contract_api(tester, code)
    ,   _symbol(sym) {}
    symbol _symbol;

    //// vesting actions
    action_result create_vesting(name creator) {
        return create_vesting(creator, _symbol, golos::config::control_name);
    }
    action_result create_vesting(name creator, symbol vesting_symbol, name notify_acc = N(notify.acc)) {
        action_result result = push(N(createvest), creator, args()
            ("symbol", vesting_symbol)
            ("notify_acc", notify_acc)
        );
        if (base_tester::success() == result) {
            _tester->link_authority(creator, _code, golos::config::invoice_name, N(retire));
        }
        return result;
    }

    action_result open(name owner) {
        return open(owner, _symbol, owner);
    }
    action_result open(name owner, symbol sym, name ram_payer) {
        return push(N(open), ram_payer, args()
            ("owner", owner)
            ("symbol", sym)
            ("ram_payer", ram_payer)
        );
    }

    action_result unlock_limit(name owner, asset quantity) {
        return push(N(unlocklimit), owner, args()
            ("owner", owner)
            ("quantity", quantity)
        );
    }

    action_result convert_vesting(name sender, name recipient, asset quantity) {
        return push(N(convertvg), sender, args()
            ("sender", sender)
            ("recipient", recipient)
            ("quantity", quantity)
        );
    }

    action_result cancel_convert_vesting(name sender, asset type) {
        return push(N(cancelvg), sender, args()
            ("sender", sender)
            ("type", type)
        );
    }

    action_result delegate_vesting(name sender, name recipient, asset quantity,
        uint16_t interest_rate = 0, uint8_t payout_strategy = 0
    ) {
        return push(N(delegatevg), sender, args()
            ("sender", sender)
            ("recipient", recipient)
            ("quantity", quantity)
            ("interest_rate", interest_rate)
            ("payout_strategy", payout_strategy)
        );
    }

    action_result undelegate_vesting(name sender, name recipient, asset quantity) {
        return push(N(undelegatevg), sender, args()
            ("sender", sender)
            ("recipient", recipient)
            ("quantity", quantity)
        );
    }

    action_result set_params(name creator, symbol symbol, std::string json_params) {
        return push(N(setparams), creator, args()
            ("symbol", symbol)
            ("params", json_str_to_obj(json_params)));
    }

    action_result timeout(name signer) {
        return push(N(timeout), signer, args()("hash", 1));
    }

    //// vesting tables
    variant get_vesting() {
        // converts assets to strings; TODO: generalize
        auto v = get_struct(_code, N(vesting), _symbol.to_symbol_code().value, "balance_vesting");
        if (v.is_object()) {
            auto o = mvo(v);
            o["supply"] = o["supply"].as<asset>().to_string();
            v = o;
        }
        return v;
    }

    variant get_balance(name acc) {
        // converts assets to strings; TODO: generalize
        auto v = get_struct(acc, N(accounts), _symbol.to_symbol_code().value, "user_balance");
        if (v.is_object()) {
            auto o = mvo(v);
            o["vesting"] = o["vesting"].as<asset>().to_string();
            o["delegate_vesting"] = o["delegate_vesting"].as<asset>().to_string();
            o["received_vesting"] = o["received_vesting"].as<asset>().to_string();
            o["unlocked_limit"] = o["unlocked_limit"].as<asset>().to_string();
            v = o;
        }
        return v;
    }

    variant get_balance_raw(name acc) {
        // base_api_helper knows code
        return get_struct(acc, N(accounts), _symbol.to_symbol_code().value, "user_balance");
    }

    std::vector<variant> get_balances(name user) {
        return _tester->get_all_chaindb_rows(_code, user, N(accounts), false);
    }

    variant get_convert_obj(name from) {
        return get_struct(_symbol.to_symbol_code().value, N(convert), from, "convert_of_tokens");
    }

    //// helpers
    asset make_asset(double x = 0, symbol sym = symbol(0)) const {
        if (sym == symbol(0)) sym = _symbol;
        return asset(x * sym.precision(), sym);
    }
    string asset_str(double x = 0) {
        return make_asset(x).to_string();
    }

    variant make_balance(double vesting, double delegated = 0, double received = 0, double unlocked = 0) {
        return mvo()
            ("vesting", asset_str(vesting))
            ("delegate_vesting", asset_str(delegated))
            ("received_vesting", asset_str(received))
            ("unlocked_limit", asset_str(unlocked));
    }

    variant get_params(symbol symbol) const {
        return base_contract_api::get_struct(symbol.to_symbol_code().value, N(vstngparams), N(vstngparams), "vesting_state");
    }

    string vesting_withdraw(uint32_t intervals, uint32_t interval_seconds) {
        return string("['vesting_withdraw', {'intervals':'") + std::to_string(intervals) + "','interval_seconds':'" + std::to_string(interval_seconds) + "'}]";
    }

    string vesting_amount(uint64_t min_amount) {
        return string("['vesting_amount', {'min_amount':'") + std::to_string(min_amount) + "'}]";
    }

    string delegation(uint64_t min_amount, uint64_t min_remainder, uint32_t min_time,
                      uint16_t max_interest, uint32_t return_time) {
        return string("['delegation', {'min_amount':'") + std::to_string(min_amount) + "','min_remainder':'" + std::to_string(min_remainder) +
                "','min_time':'" + std::to_string(min_time) + "','max_interest':'" + std::to_string(max_interest) +
                "','return_time':'" + std::to_string(return_time) + "'}]";
    }

};


}} // eosio::testing
