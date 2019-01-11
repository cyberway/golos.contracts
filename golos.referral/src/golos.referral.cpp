#include "golos.referral/golos.referral.hpp"
#include <eosiolib/transaction.hpp>

namespace golos {

using namespace eosio;

struct referral_params_setter: set_params_visitor<referral_state> {
    using set_params_visitor::set_params_visitor; // enable constructor

   bool operator()(const breakout_param& p) {
       return set_param(p, &referral_state::breakout_params);
   }

    bool operator()(const expire_param& p) {
       return set_param(p, &referral_state::expire_params);
   }

    bool operator()(const percent_param& p) {
       return set_param(p, &referral_state::percent_params);
   }

    bool operator()(const delay_param& p) {
       return set_param(p, &referral_state::delay_params);
   }
};

void referral::validateprms(std::vector<referral_params> params) {
}

void referral::setparams(std::vector<referral_params> params) {
    require_auth(_self);

    referral_params_singleton cfg(_self, _self.value);
    param_helper::set_parameters<referral_params_setter>(params, cfg, _self);
}

void referral::addreferral(name referrer, name referral, uint32_t percent,
                           uint64_t expire, asset breakout) {
    require_auth(referrer);

    referrals_table referrals(_self, _self.value);
    auto it_referral = referrals.find(referral.value);
    eosio_assert(it_referral == referrals.end(), "A referral with the same name already exists");

    eosio_assert(referrer != referral, "referral can not be referrer");

    referral_params_singleton cfg(_self, _self.value);
    eosio_assert(cfg.exists(), "not found parametrs");
    
    const auto min_expire = now();
    const auto max_expire = now() + cfg.get().expire_params.max_expire;
    eosio_assert(expire   >  min_expire, "expire < current block time");
    eosio_assert(expire   <= max_expire, "expire > current block time + max_expire");
    eosio_assert(breakout >  cfg.get().breakout_params.min_breakout, "breakout <= min_breakout");
    eosio_assert(breakout <= cfg.get().breakout_params.max_breakout, "breakout > max_breakout");
    eosio_assert(percent  <= cfg.get().percent_params.max_perсent, "specified parameter is greater than limit");
 
    referrals.emplace(referrer, [&]( auto &item ) {
        item.referral = referral;
        item.referrer = referrer;
        item.percent  = percent;
        item.expire   = expire;
        item.breakout = breakout;
    });
}

void referral::closeoldref(uint64_t hash) {
    require_auth(_self);

    referral_params_singleton cfg(_self, _self.value);
    eosio_assert(cfg.exists(), "not found parametrs");

    referrals_table referrals(_self, _self.value);
    auto index_expire = referrals.get_index<"expirekey"_n>(); 
    auto it_index_expire = index_expire.begin();
    while ( it_index_expire != index_expire.end() ) {
        if ( it_index_expire->expire < now() ) {
            it_index_expire = index_expire.erase(it_index_expire);
        } else break;
    }

    transaction trx;
    trx.actions.emplace_back( action(permission_level(_self, config::active_name), _self, "closeoldref"_n, st_hash{now()}) );
    trx.delay_sec = cfg.get().delay_params.delay_clear_old_ref;
    trx.send(_self.value, _self);
}

}

EOSIO_DISPATCH(golos::referral, (addreferral)(validateprms)(setparams)(closeoldref));
