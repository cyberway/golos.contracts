#pragma once
#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/time.hpp>


namespace golos {

using namespace eosio;

namespace config {
static const auto min_breakout = eosio::asset(0,     symbol("GLS", 4));
static const auto max_breakout = eosio::asset(50000, symbol("GLS", 4));
}

class referral: public contract {
public:
    using contract::contract;

    [[eosio::action]]
    void addreferral(name referrer, name referral, uint32_t percent, uint64_t expire, asset breakout);

private:
    struct obj_referral {
        name referrer;
        name referral;
        uint32_t percent;
        uint64_t expire;
        asset breakout;

        uint64_t primary_key()const  { return referral.value; }
        uint64_t referrer_key()const { return referrer.value; }
    };
    using referrals_table = eosio::multi_index< "referrals"_n, obj_referral,
                            eosio::indexed_by< "referrerkey"_n, eosio::const_mem_fun<obj_referral, uint64_t, &obj_referral::referrer_key> >>;

};

} // golos
