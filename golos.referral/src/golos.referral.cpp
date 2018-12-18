#include "golos.referral/golos.referral.hpp"
#include "golos.referral/config.hpp"

namespace golos {

using namespace eosio;

void referral::addreferral(name referrer, name referral, uint32_t percent,
                           uint64_t expire, asset breakout) {
    require_auth(referrer);

    referrals referrals_table(_self, _self.value);
    auto it_referral = referrals_table.find(referral.value);
    eosio_assert(it_referral == referrals_table.end(), "This referral exits");

    eosio_assert(referrer != referral, "referrer is not equal to referral");

    eosio_assert(expire > config::min_expire, "expire <= min_expire");
    eosio_assert(expire >= config::max_expire, "expire > max_expire");

    eosio_assert(breakout > config::min_breakout, "breakout <= min_breakout");
    eosio_assert(breakout <= config::max_breakout, "breakout > max_breakout");

    eosio_assert(percent > 10000, "percentages above 100%");
    eosio_assert(percent <= config::max_perсent, "specified parameter is greater than limit");

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
