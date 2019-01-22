#pragma once
#include "test_api_helper.hpp"
#include "../common/config.hpp"

namespace eosio { namespace testing {

class extended_tester : public golos_tester {
    using golos_tester::golos_tester;
    fc::microseconds _cur_time;

protected:
    const fc::microseconds& cur_time()const { return _cur_time; };
    void update_cur_time() { _cur_time = control->head_block_time().time_since_epoch();};

public:
    void step(uint32_t n = 1) {
        produce_blocks(n);
        update_cur_time();
    }

    void run(const fc::microseconds& t) {
        _produce_block(t);  // it produces only 1 block. this can cause expired transactions. use step() to push current txs into blockchain
        update_cur_time();
    }
};

struct golos_referral_api: base_contract_api {
    golos_referral_api(extended_tester* tester, name code)
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

     action_result close_old_referrals(uint64_t hash) {
         return push(N(closeoldref), _code, args()
             ("hash", hash)
         );
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
