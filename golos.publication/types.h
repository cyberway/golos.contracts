#pragma once

#ifdef UNIT_TEST_ENV
#   define ABI_TABLE
#   include <eosio/chain/types.hpp>
#   include "../golos.charge/types.h"
namespace eosio { namespace testing {
    using namespace eosio::chain;
#else
#   include <golos.charge/types.h>
#   define ABI_TABLE [[eosio::table]]
#endif

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

#ifdef UNIT_TEST_ENV
}} // eosio::testing
FC_REFLECT(eosio::testing::limitedact, (chargenum)(restorernum)(cutoffval)(chargeprice))
FC_REFLECT(eosio::testing::limitsarg, (restorers)(limitedacts)(vestingprices)(minvestings))
#endif

