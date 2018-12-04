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
    action_result create_vesting(account_name creator, std::vector<account_name> issuers = {}) {
        return create_vesting(creator, _symbol, issuers);
    }
    action_result create_vesting(account_name creator, symbol vesting_symbol, std::vector<account_name> issuers = {}) {
        authority auth(1, {});
        for(auto issuer : issuers)
            auth.accounts.emplace_back(permission_level_weight{.permission = {issuer, N(eosio.code)}, .weight = 1});
            
        if(std::find(issuers.begin(), issuers.end(), creator) == issuers.end())
            auth.accounts.emplace_back(permission_level_weight{.permission = {creator, N(eosio.code)}, .weight = 1});
            
        std::sort(auth.accounts.begin(), auth.accounts.end(),
            [](const permission_level_weight& l, const permission_level_weight& r) {
                return std::tie(l.permission.actor, l.permission.permission) < std::tie(r.permission.actor, r.permission.permission);
            });
        _tester->set_authority(creator, golos::config::invoice_name, auth, "owner");
        _tester->link_authority(creator, _code, golos::config::invoice_name, N(retire));

        return push(N(createvest), creator, args()
            ("symbol", vesting_symbol)
            ("notify_acc", account_name(golos::config::control_name))
        );
    }

    action_result open(account_name owner) {
        return open(owner, _symbol, owner);
    }
    action_result open(account_name owner, symbol sym, account_name ram_payer) {
        return push(N(open), ram_payer, args()
            ("owner", owner)
            ("symbol", sym)
            ("ram_payer", ram_payer)
        );
    }

    action_result unlock_limit(account_name owner, asset quantity) {
        return push(N(unlocklimit), owner, args()
            ("owner", owner)
            ("quantity", quantity)
        );
    }

    action_result convert_vesting(account_name sender, account_name recipient, asset quantity) {
        return push(N(convertvg), sender, args()
            ("sender", sender)
            ("recipient", recipient)
            ("quantity", quantity)
        );
    }

    action_result cancel_convert_vesting(account_name sender, asset type) {
        return push(N(cancelvg), sender, args()
            ("sender", sender)
            ("type", type)
        );
    }

    action_result delegate_vesting(account_name sender, account_name recipient, asset quantity,
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

    action_result undelegate_vesting(account_name sender, account_name recipient, asset quantity) {
        return push(N(undelegatevg), sender, args()
            ("sender", sender)
            ("recipient", recipient)
            ("quantity", quantity)
        );
    }

    action_result timeout(account_name signer) {
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

    variant get_balance(account_name acc) {
        // converts assets to strings; TODO: generalize
        auto v = get_struct(acc, N(balances), _symbol.to_symbol_code().value, "user_balance");
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

    variant get_balance_raw(account_name acc) {
        // base_api_helper knows code
        return get_struct(acc, N(balances), _symbol.to_symbol_code().value, "user_balance");
    }

    std::vector<variant> get_balances(account_name user) {
        return _tester->get_all_chaindb_rows(_code, user, N(balances), false);
    }

    variant get_convert_obj(account_name from) {
        return get_struct(_symbol.to_symbol_code().value, N(converttable), from, "convert_of_tokens");
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

};


}} // eosio::testing
