#include "golos.referral/golos.referral.hpp"

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

    bool operator()(const persent_param& p) {
       return set_param(p, &referral_state::persent_params);
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

    referrals referrals_table(_self, _self.value);
    auto it_referral = referrals_table.find(referral.value);
    eosio_assert(it_referral == referrals_table.end(), "This referral exits");

    eosio_assert(referrer != referral, "referrer is not equal to referral");

    // eosio_assert(expire > config::min_expire, "expire <= min_expire");
    // eosio_assert(expire >= config::max_expire, "expire > max_expire");

    // eosio_assert(breakout > config::min_breakout, "breakout <= min_breakout");
    // eosio_assert(breakout <= config::max_breakout, "breakout > max_breakout");

    // eosio_assert(percent > 10000, "percentages above 100%");
    // eosio_assert(percent <= config::max_perсent, "specified parameter is greater than limit");

    referrals_table.emplace(referrer, [&]( auto &item ) {
        item.id = referrals_table.available_primary_key();
        item.referrer = referrer;
        item.referral = referral;
        item.percent  = percent;
        item.expire   = expire;
        item.breakout = breakout;
    });
}

}

EOSIO_DISPATCH(golos::referral, (addreferral));
