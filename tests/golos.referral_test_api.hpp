#pragma once
#include "test_api_helper.hpp"
#include "../common/config.hpp"
#include "contracts.hpp"

namespace eosio { namespace testing {

namespace cfg = golos::config;

struct golos_referral_api: base_contract_api {
    golos_referral_api(golos_tester* tester, name code)
        :   base_contract_api(tester, code) {}

    void initialize_contract(name token_name) {
        _tester->install_contract(_code, contracts::referral_wasm(), contracts::referral_abi());

        _tester->set_authority(_code, cfg::code_name, create_code_authority({_code}), "active");
        _tester->link_authority(_code, _code, cfg::code_name, N(closeoldref));
        _tester->link_authority(_code, token_name, cfg::code_name, N(transfer));
    }

    //// referral actions
     action_result create_referral(name referrer, name referral, uint32_t percent, uint64_t expire, asset breakout) {
         return push(N(addreferral), _code, args()
             ("referrer", referrer)
             ("referral", referral)
             ("percent", percent)
             ("expire", expire)
             ("breakout", breakout)
         );
     }

     action_result close_old_referrals() {
         return push(N(closeoldref), _code, args());
     }

     action_result set_params(name creator, std::string json_params) {
         return push(N(setparams), creator, args()
             ("params", json_str_to_obj(json_params)));
     }

     variant get_params() const {
         return base_contract_api::get_struct(_code, N(refparams), N(refparams), "referral_state");
     }

    variant get_referral(name referral) {
        return _tester->get_chaindb_struct(_code, _code, N(referrals), referral, "obj_referral");
    }

     vector<variant> get_referrals() {
         return _tester->get_all_chaindb_rows(_code, _code, N(referrals), false);
     }

     string breakout_parametrs(asset min_breakout, asset max_breakout) {
         return string("['breakout_parametrs', {'min_breakout':'") + min_breakout.to_string() + "','max_breakout':'" + max_breakout.to_string() + "'}]";
     }

     string expire_parametrs(uint64_t max_expire) {
         return string("['expire_parametrs', {'max_expire':'") + std::to_string(max_expire) + "'}]";
     }

     string percent_parametrs(uint32_t max_percent) {
         return string("['percent_parametrs', {'max_percent':'") + std::to_string(max_percent) + "'}]";
     }

     string delay_parametrs(uint32_t delay_clear_old_ref) {
         return string("['delay_parametrs', {'delay_clear_old_ref':'") + std::to_string(delay_clear_old_ref) + "'}]";
     }
};


}} // eosio::testing
