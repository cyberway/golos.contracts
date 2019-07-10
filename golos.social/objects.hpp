#pragma once

#include <eosio/eosio.hpp>
#include "types.h"
#include <eosio/singleton.hpp>

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

} // structures

namespace tables {

using namespace eosio;

using pinblock_table = multi_index<"pinblock"_n, structures::pinblock_record>;
}


} // golos
