#pragma once
#include <eosio/eosio.hpp>
#include <eosio/time.hpp>
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
    void closeoldref();

    void on_transfer(name from, name to, asset quantity, std::string memo);

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

        bool is_empty() const {
            auto status = referral == name() && referrer == name();
            return status;
        }
    };
    using referrals_table = eosio::multi_index< "referrals"_n, obj_referral,
                            eosio::indexed_by< "referrerkey"_n, eosio::const_mem_fun<obj_referral, uint64_t, &obj_referral::referrer_key> >,
                            eosio::indexed_by< "expirekey"_n, eosio::const_mem_fun<obj_referral, uint64_t, &obj_referral::expire_key> > >;

public:
    static inline obj_referral account_referrer( name contract_name, name referral ) {
        referrals_table referrals( contract_name, contract_name.value );
        auto itr = referrals.find( referral.value );
        if (itr != referrals.end()) {
            const auto now = static_cast<uint64_t>(eosio::current_time_point().sec_since_epoch());
            if (itr->expire >= now) {
                return *itr;
            }
        }

        return obj_referral();
    }

};

} // golos
