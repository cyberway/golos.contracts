#pragma once

#include <common/config.hpp>

#ifdef UNIT_TEST_ENV
#   include <eosio/chain/types.hpp>
namespace eosio { namespace testing {
    using namespace eosio::chain;
#   include <fc/optional.hpp>
    using fc::optional;
#else
#   include <eosio/eosio.hpp>
    using namespace eosio;
    using std::optional;
#endif

struct accountmeta {
    accountmeta() = default;

    optional<std::string> type; // ?

    optional<std::string> app;

    optional<std::string> email;
    optional<std::string> phone;
    optional<std::string> facebook;
    optional<std::string> instagram;
    optional<std::string> telegram;
    optional<std::string> vk;
    optional<std::string> whatsapp;
    optional<std::string> wechat;
    optional<std::string> website;

    optional<std::string> first_name;
    optional<std::string> last_name;
    optional<std::string> name;
    optional<std::string> birth_date;
    optional<std::string> gender;
    optional<std::string> location;
    optional<std::string> city;

    optional<std::string> about;
    optional<std::string> occupation;
    optional<std::string> i_can;
    optional<std::string> looking_for;
    optional<std::string> business_category; // ?

    optional<std::string> background_image;
    optional<std::string> cover_image;
    optional<std::string> profile_image;
    optional<std::string> user_image;
    optional<std::string> ico_address;

    // ?
    optional<std::string> target_date;
    optional<std::string> target_plan;
    optional<std::string> target_point_a;
    optional<std::string> target_point_b;

#ifndef UNIT_TEST_ENV
    EOSLIB_SERIALIZE(accountmeta,
        (type)(app)(email)(phone)(facebook)(instagram)(telegram)(vk)(whatsapp)(wechat)(website)
        (first_name)(last_name)(name)(birth_date)(gender)(location)(city)(about)(occupation)(i_can)(looking_for)
        (business_category)(background_image)(cover_image)(profile_image)(user_image)(ico_address)(target_date)
        (target_plan)(target_point_a)(target_point_b))
#endif
};

#ifdef UNIT_TEST_ENV
}} // eosio::testing
FC_REFLECT(eosio::testing::accountmeta,
    (type)(app)(email)(phone)(facebook)(instagram)(telegram)(vk)(whatsapp)(wechat)(website)
    (first_name)(last_name)(name)(birth_date)(gender)(location)(city)(about)(occupation)(i_can)(looking_for)
    (business_category)(background_image)(cover_image)(profile_image)(user_image)(ico_address)(target_date)
    (target_plan)(target_point_a)(target_point_b))
#endif
