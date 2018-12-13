#pragma once
#include <eosiolib/eosio.hpp>
#include <eosiolib/time.hpp>


namespace golos {

using namespace eosio;

class referral: public contract {
public:
    using contract::contract;

    [[eosio::action]]
    void crtreferral(name referrer, name referral, uint32_t percent, uint64_t expire, uint64_t breakout);

private:
    struct obj_referral {
        name referral;
        uint32_t percent;
        uint64_t expire;
        uint64_t breakout;

        uint64_t primary_key()const { return referral.value; }
    };
    using referrals = eosio::multi_index<"referrals"_n, obj_referral>;

};

} // golos
