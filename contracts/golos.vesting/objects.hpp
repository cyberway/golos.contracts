#pragma once
#include "config.hpp"

#include <eosiolib/time.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>

using namespace eosio;

namespace structures {

struct token_vesting
{
    token_vesting() = default;

    uint64_t id;
    asset vesting;
    asset token;

    uint64_t primary_key() const {
        return id;
    }

    uint64_t vesting_key() const {
        return vesting.symbol.name();
    }

    uint64_t token_key() const {
        return token.symbol.name();
    }

    EOSLIB_SERIALIZE(token_vesting, (id)(vesting)(token))
};

struct user_battery
{
    user_battery() = default;

    uint64_t charge;
    time_point_sec renewal;

    EOSLIB_SERIALIZE(user_battery, (charge)(renewal))
};

struct user_balance
{
    user_balance() = default;

    asset vesting;
    asset delegate_vesting;
    asset received_vesting;
    user_battery battery;

    uint64_t primary_key() const {
        return vesting.symbol.name();
    }

    EOSLIB_SERIALIZE(user_balance, (vesting)(delegate_vesting)(received_vesting)(battery))
};

struct delegate_record
{
    delegate_record() = default;

    uint64_t id;
    account_name sender;
    account_name recipient;
    asset quantity;
    asset deductions;
    uint16_t percentage_deductions;
    time_point_sec return_date;

    auto primary_key() const {
        return id;
    }

    auto sender_key() const {
        return sender;
    }

    auto recipient_key() const {
        return recipient;
    }

    EOSLIB_SERIALIZE(delegate_record, (id)(sender)(recipient)(quantity)
                     (deductions)(percentage_deductions)(return_date))
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

struct issue_vesting {
    issue_vesting() = default;

    account_name to;
    asset quantity;
    EOSLIB_SERIALIZE( issue_vesting, (to)(quantity) )
};
}

namespace tables {
    using index_tokens  = indexed_by<N(token),  const_mem_fun<structures::token_vesting, uint64_t, &structures::token_vesting::token_key>>;
    using index_vesting = indexed_by<N(vesting), const_mem_fun<structures::token_vesting, uint64_t, &structures::token_vesting::vesting_key>>;
    using vesting_table = eosio::multi_index<N(vesting), structures::token_vesting, index_vesting, index_tokens>;

    using account_table = eosio::multi_index<N(balances), structures::user_balance>;

    using index_sender = indexed_by<N(sender), const_mem_fun<structures::delegate_record, uint64_t, &structures::delegate_record::sender_key>>;
    using index_recipient = indexed_by<N(recipient), const_mem_fun<structures::delegate_record, uint64_t, &structures::delegate_record::recipient_key>>;
    using delegate_table = eosio::multi_index<N(delegate), structures::delegate_record, index_sender, index_recipient>;

    using index_date = indexed_by<N(date), const_mem_fun<structures::return_delegate, uint64_t, &structures::return_delegate::date_key>>;
    using return_delegate_table = eosio::multi_index<N(rdelegate), structures::return_delegate, index_date>;

    using index_payout_time = indexed_by<N(payout_time), const_mem_fun<structures::convert_of_tokens, uint64_t, &structures::convert_of_tokens::payout_time_key>>;
    using convert_table = eosio::multi_index<N(converttable), structures::convert_of_tokens, index_payout_time>;
}

