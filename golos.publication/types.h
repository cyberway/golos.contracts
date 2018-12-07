#pragma once

#include <common/config.hpp>

#ifdef UNIT_TEST_ENV
#   define ABI_TABLE
#   include <eosio/chain/types.hpp>
namespace eosio { namespace testing {
    using namespace eosio::chain;
#else
#   define ABI_TABLE [[eosio::table]]
#   include <eosiolib/types.h>
#endif

using enum_t = uint8_t;
using base_t = int64_t;// !if you change it -- don't forget about the abi-file
using wide_t = int128_t;
constexpr int fixed_point_fractional_digits = 12;

struct ABI_TABLE funcparams {
    std::string str;
    base_t maxarg;
};

constexpr uint8_t disabled_restorer = std::numeric_limits<uint8_t>::max();

struct ABI_TABLE limitedact {
    uint8_t chargenum;
    uint8_t restorernum;
    base_t cutoffval;
    base_t chargeprice;
};

struct limitsarg {
    std::vector<std::string> restorers;
    std::vector<limitedact> limitedacts;
    std::vector<int64_t> vestingprices;
    std::vector<int64_t> minvestings;
};

enum class payment_t: enum_t { TOKEN, VESTING };

struct forumprops {
    forumprops() = default;

    name contract_for_reputation = name();
};

#ifdef UNIT_TEST_ENV
}} // eosio::testing
FC_REFLECT(eosio::testing::limitedact, (chargenum)(restorernum)(cutoffval)(chargeprice))
FC_REFLECT(eosio::testing::limitsarg, (restorers)(limitedacts)(vestingprices)(minvestings))
FC_REFLECT(eosio::testing::forumprops, (contract_for_reputation))
#endif

#ifdef UNIT_TEST_ENV
#   include "../common/calclib/fixed_point_utils.h"
#else
#   include <common/calclib/fixed_point_utils.h>
#endif
