#pragma once
#include <eosiolib/eosio.hpp>
#include <eosiolib/time.hpp>
#include "parameters.hpp"

namespace golos {

using namespace eosio;

class referral: public contract {
public:
    using contract::contract;

    [[eosio::action]]
    void validateprms(std::vector<referral_params> params);

    [[eosio::action]]
    void setparams(std::vector<referral_params> params);

    [[eosio::action]]
    void addreferral(name referrer, name referral, uint32_t percent, uint64_t expire, asset breakout);

    [[eosio::action]]
    void closeoldref(uint64_t hash);

private:
    struct obj_referral {
        name referrer;
        name referral;
        uint32_t percent;
        uint64_t expire;
        asset breakout;

        uint64_t primary_key()const  { return referral.value; }
        uint64_t referrer_key()const { return referrer.value; }
        uint64_t expire_key()const { return expire; }
    };
    using referrals_table = eosio::multi_index< "referrals"_n, obj_referral,
                            eosio::indexed_by< "referrerkey"_n, eosio::const_mem_fun<obj_referral, uint64_t, &obj_referral::referrer_key> >,
                            eosio::indexed_by< "expirekey"_n, eosio::const_mem_fun<obj_referral, uint64_t, &obj_referral::expire_key> > >;

    struct st_hash {
        uint64_t hash;
    };

};

} // golos
