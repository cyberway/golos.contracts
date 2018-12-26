#pragma once
#include "test_api_helper.hpp"
#include "../common/config.hpp"

namespace eosio { namespace testing {


struct golos_referral_api: base_contract_api {
    golos_referral_api(golos_tester* tester, name code)
        :   base_contract_api(tester, code) {}

    //// referral actions
     action_result create_referral(name referrer, name referral, uint32_t percent, uint64_t expire, asset breakout) {
         return push(N(addreferral), referrer, args()
             ("referrer", referrer)
             ("referral", referral)
             ("percent", percent)
             ("expire", expire)
             ("breakout", breakout)
         );
     }

     action_result set_params(name creator, std::string json_params) {
         return push(N(setparams), creator, args()
             ("params", json_str_to_obj(json_params)));
     }

     variant get_params() const {
         return base_contract_api::get_struct(_code, N(refparams), N(refparams), "referral_state");
     }

     string breakout_parametrs(asset min_breakout, asset max_breakout) {
         return string("['breakout_parametrs', {'min_breakout':'") + min_breakout.to_string() + "','max_breakout':'" + max_breakout.to_string() + "'}]";
     }

     string expire_parametrs(uint64_t max_expire) {
         return string("['expire_parametrs', {'max_expire':'") + std::to_string(max_expire) + "'}]";
     }

     string percent_parametrs(uint32_t max_perсent) {
         return string("['percent_parametrs', {'max_perсent':'") + std::to_string(max_perсent) + "'}]";
     }
};


}} // eosio::testing
