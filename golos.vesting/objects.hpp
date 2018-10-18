#pragma once
#include "config.hpp"

#include <eosiolib/time.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>

using namespace eosio;

namespace structures {

struct vesting_info
{
    vesting_info() = default;

    asset supply;
    std::vector<account_name> issuers;

    uint64_t primary_key() const {
        return supply.symbol.name();
    }

    EOSLIB_SERIALIZE(vesting_info, (supply)(issuers))
};

struct user_balance
{
    user_balance() = default;

    asset vesting;
    asset delegate_vesting;
    asset received_vesting;
    asset unlocked_limit;

    uint64_t primary_key() const {
        return vesting.symbol.name();
    }
    
    asset available_vesting() const {
        return vesting - delegate_vesting;
    }
    
    asset effective_vesting() const {
        return (vesting - delegate_vesting) + received_vesting; 
    }
    
    asset unlocked_vesting() const {
        return std::min(available_vesting(), unlocked_limit);
    }

    EOSLIB_SERIALIZE(user_balance, (vesting)(delegate_vesting)(received_vesting)(unlocked_limit))
};

struct delegate_record
{
    delegate_record() = default;

    uint64_t id;
    account_name sender;
    account_name recipient;
    asset quantity;
    asset deductions;
    uint16_t interest_rate;
    uint16_t payout_strategy;
    time_point_sec return_date;

    static uint128_t unique_key(account_name u_sender, account_name u_recipient) {
        return (uint128_t)u_sender + (((uint128_t)u_recipient)<<64);
    }

    auto primary_key() const {
        return id;
    }

    auto secondary_key() const {
        return unique_key(sender, recipient);
    }

    auto sender_key() const {
        return sender;
    }

    auto recipient_key() const {
        return recipient;
    }

    EOSLIB_SERIALIZE(delegate_record, (id)(sender)(recipient)(quantity)
                     (deductions)(interest_rate)(payout_strategy)(return_date))
};

struct return_delegate
{
    return_delegate() = default;

    uint64_t id;
    account_name recipient;
    asset amount;
    time_point_sec date;

    uint64_t primary_key() const {
        return id;
    }

    uint64_t date_key() const {
        return uint64_t(date.utc_seconds);
    }

    EOSLIB_SERIALIZE(return_delegate, (id)(recipient)(amount)(date))
};

struct convert_of_tokens {
    convert_of_tokens() = default;

    account_name sender;
    account_name recipient;
    uint8_t number_of_payments;
    time_point_sec payout_time;
    asset payout_part;
    asset balance_amount;

    uint64_t primary_key() const {
        return sender;
    }

    uint64_t payout_time_key() const {
        return uint64_t(payout_time.utc_seconds);
    }

    EOSLIB_SERIALIZE( convert_of_tokens, (sender)(recipient)(number_of_payments)(payout_time)
                      (payout_part)(balance_amount) )
};

struct shash {
    uint64_t hash;

    EOSLIB_SERIALIZE( shash, (hash) )
};
}

namespace tables {
    using vesting_table = eosio::multi_index<N(vesting), structures::vesting_info>;

    using account_table = eosio::multi_index<N(balances), structures::user_balance>;

    using index_sender = indexed_by<N(sender), const_mem_fun<structures::delegate_record, uint64_t, &structures::delegate_record::sender_key>>;
    using index_recipient = indexed_by<N(recipient), const_mem_fun<structures::delegate_record, uint64_t, &structures::delegate_record::recipient_key>>;
    using index_unique_key = indexed_by<N(unique), const_mem_fun<structures::delegate_record, uint128_t, &structures::delegate_record::secondary_key>>;
    using delegate_table = eosio::multi_index<N(delegate), structures::delegate_record, index_sender, index_recipient, index_unique_key>;

    using index_date = indexed_by<N(date), const_mem_fun<structures::return_delegate, uint64_t, &structures::return_delegate::date_key>>;
    using return_delegate_table = eosio::multi_index<N(rdelegate), structures::return_delegate, index_date>;

    using index_payout_time = indexed_by<N(payouttime), const_mem_fun<structures::convert_of_tokens, uint64_t, &structures::convert_of_tokens::payout_time_key>>;
    using convert_table = eosio::multi_index<N(converttable), structures::convert_of_tokens, index_payout_time>;
}

