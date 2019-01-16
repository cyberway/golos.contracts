#pragma once

#include <eosiolib/eosio.hpp>
#include "types.h"
#include <eosiolib/singleton.hpp>

namespace golos {

using namespace eosio;

namespace structures {

struct pinblock_record {
    pinblock_record() = default;

    name account;
    bool pinning = false;
    bool blocking = false;

    uint64_t primary_key() const {
        return account.value;
    }

    EOSLIB_SERIALIZE(pinblock_record, (account)(pinning)(blocking))
};

struct reputation_record {
    reputation_record() = default;

    int64_t reputation = 0;

    EOSLIB_SERIALIZE(reputation_record, (reputation))
};

} // structures

namespace tables {

using namespace eosio;

using pinblock_table = multi_index<"pinblock"_n, structures::pinblock_record>;
using reputation_singleton = eosio::singleton<"reputation"_n, structures::reputation_record>;
}


} // golos
