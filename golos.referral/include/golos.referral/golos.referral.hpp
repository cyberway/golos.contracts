#pragma once
#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/time.hpp>


namespace golos {

using namespace eosio;

class referral: public contract {
public:
    using contract::contract;

    [[eosio::action]]
    void addreferral(name referrer, name referral, uint32_t percent, uint64_t expire, asset breakout);

private:
    struct obj_referral {
        uint64_t id;
        name referrer;
        name referral;
        uint32_t percent;
        uint64_t expire;
        asset breakout;

        uint64_t primary_key()const  { return id; }
        uint64_t referrer_key()const { return referrer.value; }
        uint64_t referral_key()const { return referral.value; }
    };
    using referrals = eosio::multi_index< "referrals"_n, obj_referral,
                      eosio::indexed_by< "referrerkey"_n, eosio::const_mem_fun<obj_referral, uint64_t, &obj_referral::referrer_key> >,
                      eosio::indexed_by< "referralkey"_n, eosio::const_mem_fun<obj_referral, uint64_t, &obj_referral::referral_key> >>;

};

} // golos
