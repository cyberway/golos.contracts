#include "golos.referal/golos.referal.hpp"

namespace golos {

using namespace eosio;

void referal::crtreferal(name referrer, name referral, name expire,
                         uint32_t percent, time_point_sec breakout) {
    require_auth(referrer);
}

}

EOSIO_DISPATCH(golos::referal, (crtreferal));
