#include "golos.referral/golos.referral.hpp"
#include "golos.referral/config.hpp"

namespace golos {

using namespace eosio;

void referral::addreferral(name referrer, name referral, uint32_t percent,
                           uint64_t expire, asset breakout) {
    require_auth(referrer);

    referrals_table referrals(_self, _self.value);
    auto it_referral = referrals.find(referral.value);
    eosio_assert(it_referral == referrals.end(), "This referral exits");

    eosio_assert(referrer != referral, "referrer is not equal to referral");

    const auto min_expire = now();
    const auto max_expire = now() + config::max_expire;

    eosio_assert(expire > min_expire, "expire < current block time");
    eosio_assert(expire <= max_expire, "expire > current block time + max_expire");

    eosio_assert(breakout > config::min_breakout, "breakout <= min_breakout");
    eosio_assert(breakout <= config::max_breakout, "breakout > max_breakout");

    eosio_assert(percent <= config::max_perсent, "specified parameter is greater than limit");
 
    referrals.emplace(referrer, [&]( auto &item ) {
        item.referral = referral;
        item.referrer = referrer;
        item.percent  = percent;
        item.expire   = expire;
        item.breakout = breakout;
    });
}

}

EOSIO_DISPATCH(golos::referral, (addreferral));
