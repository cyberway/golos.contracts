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

    asset vesting_in_system;
    asset tokens_in_vesting;

    uint64_t primary_key() const {
        return vesting_in_system.symbol.name();
    }

    EOSLIB_SERIALIZE(token_vesting, (vesting_in_system)(tokens_in_vesting))
};

struct user_balance
{
    user_balance() = default;

    asset vesting;
    asset delegate_vesting;
    asset received_vesting;

    uint64_t primary_key() const {
        return vesting.symbol.name();
    }

    EOSLIB_SERIALIZE(user_balance, (vesting)(delegate_vesting)(received_vesting))
};

struct delegate_record
{
    delegate_record() = default;

    uint64_t id;
    account_name recipient;
    asset quantity;
    float percentage_deductions;
    time_point_sec return_date;

    auto primary_key() const {
        return id;
    }

    auto secondary_key() const {
        return recipient;
    }

    EOSLIB_SERIALIZE(delegate_record, (recipient)(quantity)(percentage_deductions)(return_date))
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

    EOSLIB_SERIALIZE(return_delegate, (id)(recipient)(amount)(date))
};

struct transfer_vesting
{
    transfer_vesting() = default;

    account_name sender;
    account_name recipient;
    asset quantity;

    EOSLIB_SERIALIZE(transfer_vesting, (sender)(recipient)(quantity))
};

struct accrue_vesting
{
    accrue_vesting() = default;

    account_name sender;
    account_name user;
    asset quantity;

    EOSLIB_SERIALIZE(accrue_vesting, (sender)(user)(quantity))
};

struct delegate_vesting : transfer_vesting
{
    delegate_vesting() = default;
    float percentage_deductions;

    EOSLIB_SERIALIZE(delegate_vesting, (sender)(recipient)(quantity)(percentage_deductions))
};

struct convert_of_tokens {
    convert_of_tokens() = default;

    account_name sender;
    account_name recipient;
    uint8_t number_of_payments;
    uint64_t payout_period;
    asset payout_part;
    asset balance_amount;

    uint64_t primary_key() const {
        return sender;
    }

    EOSLIB_SERIALIZE( convert_of_tokens, (sender)(recipient)(number_of_payments)(payout_period)
                      (payout_part)(balance_amount) )
};
}

namespace tables {
    using vesting_table = eosio::multi_index<N(vesting), structures::token_vesting>;
    using account_table = eosio::multi_index<N(balances), structures::user_balance>;
    using index_recipient = eosio::indexed_by<N(recipient), eosio::const_mem_fun<structures::delegate_record, account_name, &structures::delegate_record::secondary_key>>;
    using delegate_table = eosio::multi_index<N(delegate), structures::delegate_record, index_recipient>;
    using return_delegate_table = eosio::multi_index<N(rdelegate), structures::return_delegate>;
    using convert_table = eosio::multi_index<N(converttable), structures::convert_of_tokens>;
}

