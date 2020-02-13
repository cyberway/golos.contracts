#pragma once
#include <eosio/eosio.hpp>
#include <eosio/time.hpp>
#include "parameters.hpp"

namespace golos {

using namespace eosio;

class [[eosio::contract("golos.referral")]] referral: public contract {
public:
    using contract::contract;

    [[eosio::action]]
    void validateprms(std::vector<referral_params> params);

    [[eosio::action]]
    void setparams(std::vector<referral_params> params);

    [[eosio::action("addreferral")]]
    void add_referral(name referrer, name referral, uint16_t percent, uint64_t expire, asset breakout);

    [[eosio::action]]
    void closeoldref();

    void on_transfer(name from, name to, asset quantity, std::string memo);

private:
    struct addreferral {
        name referrer;
        name referral;
        uint16_t percent;
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

    using key_index [[using eosio: order("referrer", "asc"), non_unique]] =
        eosio::indexed_by<"referrerkey"_n, eosio::const_mem_fun<addreferral, uint64_t, &addreferral::referrer_key>>;
    using expirekey_index [[using eosio: order("expire", "asc"), non_unique]] =
        eosio::indexed_by<"expirekey"_n, eosio::const_mem_fun<addreferral, uint64_t, &addreferral::expire_key>>;
    using referrals_table [[eosio::order("referral", "asc")]] =
        eosio::multi_index<"referrals"_n, addreferral, key_index, expirekey_index>;

public:
    static inline addreferral account_referrer(name contract_name, name referral) {
        referrals_table referrals(contract_name, contract_name.value);
        auto itr = referrals.find(referral.value);
        if (itr != referrals.end()) {
            const auto now = static_cast<uint64_t>(eosio::current_time_point().sec_since_epoch());
            if (itr->expire >= now) {
                return *itr;
            }
        }
        return addreferral();
    }

};

} // golos
