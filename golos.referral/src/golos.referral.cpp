#include "golos.referral/golos.referral.hpp"

namespace golos {

using namespace eosio;

void referral::addreferral(name referrer, name referral, uint32_t percent,
                           uint64_t expire, uint64_t breakout) {
    require_auth(referrer);

    referrals referrals_table(_self, _self.value);
    auto it_referral = referrals_table.find(referral.value);
    eosio_assert(it_referral == referrals_table.end(), "This referral exits");

    eosio_assert(referrer != referral, "referrer is not equal to referral");
    eosio_assert(expire >= now(), "expiration earlier than current time");
    eosio_assert(breakout == expire + 30*60, "destruction 30 minutes after expiration");
    eosio_assert(percent > 100, "percentages above 100%");

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
