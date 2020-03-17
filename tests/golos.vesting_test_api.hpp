#pragma once
#include "test_api_helper.hpp"
#include "../common/config.hpp"
#include "contracts.hpp"

namespace eosio { namespace testing {

namespace cfg = golos::config;

struct golos_vesting_api: base_contract_api {
    golos_vesting_api(golos_tester* tester, name code, symbol sym)
    :   base_contract_api(tester, code)
    ,   _symbol(sym) {}

    symbol _symbol;

    void initialize_contract(name token_name) {
        _tester->install_contract(cfg::vesting_name, contracts::vesting_wasm(), contracts::vesting_abi());
        _tester->set_authority(_code, cfg::code_name, create_code_authority({_code}), "active");
        _tester->link_authority(_code, _code, cfg::code_name, N(procwaiting));
        _tester->link_authority(_code, token_name, cfg::code_name, N(payment));
    }

    void add_changevest_auth_to_issuer(account_name issuer, account_name control) {
        // It's need to call control:changevest from vesting
        _tester->set_authority(issuer, cfg::changevest_name, create_code_authority({_code}), "active");
        _tester->link_authority(issuer, control, cfg::changevest_name, N(changevest));
    }

    //// vesting actions
    action_result create_vesting(name creator) {
        return create_vesting(creator, _symbol, golos::config::control_name);
    }
    action_result create_vesting(name creator, symbol vesting_symbol, name notify_acc, bool skip_authority_check = false) {
        if (!skip_authority_check) {
            BOOST_CHECK(_tester->has_code_authority(creator, cfg::changevest_name, _code));
            BOOST_CHECK(_tester->has_link_authority(creator, cfg::changevest_name, notify_acc, N(changevest)));
        }

        return push(N(create), creator, args()
            ("symbol", vesting_symbol)
            ("notify_acc", notify_acc)
        );
    }

    action_result open(name owner) {
        return open(owner, _symbol, owner);
    }
    action_result open(name owner, symbol sym, name ram_payer = {}) {
        return push(N(open), ram_payer, args()
            ("owner", owner)
            ("symbol", sym)
            ("ram_payer", ram_payer == name{} ? owner : ram_payer)
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
    
    std::vector<variant> get_delegators() {
        return _tester->get_all_chaindb_rows(_code, _symbol.to_symbol_code().value, N(delegation), false);
    }
    
    int64_t get_delegated(name from, name to) {
        auto delegators = get_delegators();
        for (auto& d : delegators) {
            if (d["delegator"].as<name>() == from && d["delegatee"].as<name>() == to) {
                return d["quantity"].as<asset>().get_amount();
            }
        }
        return 0;
    }
    
     action_result delegate_unauthorized(name from, name to, asset quantity, bool by_delegatee) {
        return push(N(delegate), by_delegatee ? from : to, args() 
            ("from", from) 
            ("to", to) 
            ("quantity", quantity) 
            ("interest_rate", 0) 
        );
    }
    
    action_result delegate(name from, name to, asset quantity, uint16_t interest_rate = 0) {
        if (from == to) {
            return push(N(delegate), from, args()
                ("from", from)
                ("to", to)
                ("quantity", quantity)
                ("interest_rate", 0)
            );
        }

        return push_msig(N(delegate), {{from, config::active_name}, {to, config::active_name}}, {from, to}, args()
            ("from", from)
            ("to", to)
            ("quantity", quantity)
            ("interest_rate", interest_rate));
    }

    action_result undelegate(name from, name to, asset quantity, bool by_delegatee = false) {
        return push(N(undelegate), by_delegatee ? to : from, args()
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

    action_result retire(asset quantity, name user, name issuer) {
        return push(N(retire), issuer, args()
            ("quantity", quantity)
            ("user", user)
        );
    }

    action_result procwaiting(symbol sym, account_name payer) {
        return push(N(procwaiting), payer, args()
            ("symbol", sym)
            ("payer", payer)
        );
    }
    action_result procwaiting(symbol sym) {
        return procwaiting(sym, _code);
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

    string delegation_param(uint64_t min_amount, uint64_t min_remainder, uint32_t min_time, uint32_t return_time, uint32_t max_delegators) {
        return string("['vesting_delegation', {'min_amount':'") + std::to_string(min_amount) +
                "','min_remainder':'" + std::to_string(min_remainder) +
                "','min_time':'" + std::to_string(min_time) +
                "','return_time':'" + std::to_string(return_time) +
                "','max_delegators':'" + std::to_string(max_delegators) + "'}]";
    }

    string bwprovider_param(account_name actor, permission_name permission) {
        return string("['vesting_bwprovider', {'actor':'") + name{actor}.to_string() +
                "','permission':'" + name{permission}.to_string() + "'}]";
    }
};


}} // eosio::testing
