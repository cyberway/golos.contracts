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

};


}} // eosio::testing
