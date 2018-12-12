#pragma once
#include <eosiolib/eosio.hpp>
#include <eosiolib/time.hpp>


namespace golos {

using namespace eosio;


// NOTE: not finished yet, delayed until we require median, etc…
class referal: public contract {
public:
    using contract::contract;

    void crtreferal(name referrer, name referral, name expire, uint32_t percent, time_point_sec breakout);

private:

};

} // golos
