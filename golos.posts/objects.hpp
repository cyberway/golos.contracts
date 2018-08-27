#pragma once

#include <eosiolib/time.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>

using namespace eosio;

namespace structures {

struct beneficiary
{
    beneficiary() = default;

    account_name account;
    float weight;

    EOSLIB_SERIALIZE(beneficiary, (account)(weight))
};

struct create_post
{
    create_post() = default;

    account_name autor;
    std::string permlink;
    uint64_t parent_id;
    uint8_t without_payment;
    std::vector<beneficiary> beneficiaries;
    account_name root;

    EOSLIB_SERIALIZE(create_post, (autor)(permlink)(parent_id)(without_payment)
                     (beneficiaries)(root))
};

struct update_post
{
    update_post() = default;

    uint64_t id;
    account_name autor;
    std::string permlink;
    uint8_t without_payment;
    std::vector<beneficiary> beneficiaries;
    account_name root;

    EOSLIB_SERIALIZE(update_post, (id)(autor)(permlink)(without_payment)
                    (beneficiaries)(root))
};

struct remove_post
{
    remove_post() = default;

    uint64_t id_post;
    account_name autor;
    std::string permlink;

    EOSLIB_SERIALIZE(remove_post, (id_post)(autor)(permlink))
};

struct post : create_post
{
    post() = default;

    time_point_sec date_create;
    time_point_sec last_update;
    std::string shares;
    asset total_weight;
    float share_token;
    uint64_t id;

    auto primary_key() const {
        return id;
    }

    EOSLIB_SERIALIZE(post, (autor)(permlink)(parent_id)(beneficiaries)(without_payment)
                     (root)(date_create)(last_update)(shares)
                     (total_weight)(share_token)(id))
};

struct vote
{
    vote() = default;

    account_name account;
    account_name autor;
    std::string permlink;

    EOSLIB_SERIALIZE(vote, (account)(autor)(permlink))
};

struct table_vote : vote
{
    table_vote() = default;

    uint64_t weight;
    uint64_t id;

    auto primary_key() const {
        return id;
    }

    auto autor_key() const {
        return autor;
    }

    EOSLIB_SERIALIZE(table_vote, (account)(autor)(permlink)(weight)(id))
};

}

namespace tables {
    using posts_table = eosio::multi_index<N(posts), structures::post>;

    using index_autor = indexed_by<N(autor), const_mem_fun<structures::table_vote, uint64_t, &structures::table_vote::autor_key>>;
    using votes_table = eosio::multi_index<N(votes), structures::table_vote, index_autor>;
}

