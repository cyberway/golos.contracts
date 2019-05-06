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
        action_result result = push(N(create), creator, args()
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

    action_result close(name owner, symbol sym) {
        return push(N(close), owner, args()
            ("owner", owner)
            ("symbol", sym)
        );
    }

    action_result unlock_limit(name owner, asset quantity) {
        return push(N(unlocklimit), owner, args()
            ("owner", owner)
            ("quantity", quantity)
        );
    }

    action_result withdraw(name from, name to, asset quantity) {
        return push(N(withdraw), from, args()
            ("from", from)
            ("to", to)
            ("quantity", quantity)
        );
    }

    action_result stop_withdraw(name owner, symbol sym) {
        return push(N(stopwithdraw), owner, args()
            ("owner", owner)
            ("symbol", sym)
        );
    }

    action_result delegate(name from, name to, asset quantity,
        uint16_t interest_rate = 0, uint8_t payout_strategy = 0
    ) {
        return push(N(delegate), from, args()
            ("from", from)
            ("to", to)
            ("quantity", quantity)
            ("interest_rate", interest_rate)
            ("payout_strategy", payout_strategy)
        );
    }

    action_result undelegate(name from, name to, asset quantity) {
        return push(N(undelegate), from, args()
            ("from", from)
            ("to", to)
            ("quantity", quantity)
        );
    }

    action_result set_params(name creator, symbol sym, std::string json_params) {
        return push(N(setparams), creator, args()
            ("symbol", sym)
            ("params", json_str_to_obj(json_params)));
    }

    action_result timeout(name signer) {
        return push(N(timeout), signer, args());
    }

    //// vesting tables
    variant get_stats() {
        // converts assets to strings; TODO: generalize
        auto v = get_struct(_code, N(stat), _symbol.to_symbol_code().value, "balance_vesting");
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
            o["delegated"] = o["delegated"].as<asset>().to_string();
            o["received"] = o["received"].as<asset>().to_string();
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

    variant get_withdraw_obj(name from) {
        return get_struct(_symbol.to_symbol_code().value, N(withdrawal), from, "withdraw_record");
    }

    // TODO: delegation

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
            ("delegated", asset_str(delegated))
            ("received", asset_str(received))
            ("unlocked_limit", asset_str(unlocked));
    }

    variant get_params(symbol symbol) const {
        return base_contract_api::get_struct(symbol.to_symbol_code().value, N(vestparams), N(vestparams), "vesting_state");
    }

    string withdraw_param(uint8_t intervals, uint32_t interval_seconds) {
        return string("['vesting_withdraw', {'intervals':'") + std::to_string(intervals) + "','interval_seconds':'" + std::to_string(interval_seconds) + "'}]";
    }

    string amount_param(uint64_t min_amount) {
        return string("['vesting_amount', {'min_amount':'") + std::to_string(min_amount) + "'}]";
    }

    string delegation_param(uint64_t min_amount, uint64_t min_remainder, uint32_t min_time,
                      uint16_t max_interest, uint32_t return_time) {
        return string("['vesting_delegation', {'min_amount':'") + std::to_string(min_amount) +
                "','min_remainder':'" + std::to_string(min_remainder) +
                "','min_time':'" + std::to_string(min_time) + "','max_interest':'" + std::to_string(max_interest) +
                "','return_time':'" + std::to_string(return_time) + "'}]";
    }

};


}} // eosio::testing
