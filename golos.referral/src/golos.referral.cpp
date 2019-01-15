#include "golos.referral/golos.referral.hpp"
#include <eosio.token/eosio.token.hpp>
#include <golos.vesting/config.hpp>
#include <common/dispatchers.hpp>

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

    print("current tume: ",uint64_t(now()));
    
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

void referral::on_transfer(name from, name to, asset quantity, std::string memo) {
    if(_self != to)
        return;

    referrals_table referrals(_self, _self.value);
    auto it_referral = referrals.find(from.value);
    eosio_assert(it_referral != referrals.end(), "A referral with this name doesn't exist.");
    eosio_assert(it_referral->breakout.amount == quantity.amount, "Amount of funds doesn't equal.");
    
    INLINE_ACTION_SENDER(eosio::token, transfer)
        (config::token_name, {_self, config::active_name},
        {_self, it_referral->referrer, quantity, ""});

    referrals.erase(it_referral);
}

}

DISPATCH_WITH_TRANSFER(golos::referral, on_transfer, (addreferral)(validateprms)(setparams))
