#pragma once

#include <eosiolib/eosio.hpp>

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

using pb_account_idx = indexed_by<"account"_n, const_mem_fun<structures::pinblock_record, uint64_t, &structures::pinblock_record::primary_key>>;
using pinblock_table = multi_index<"pinblock"_n, structures::pinblock_record, pb_account_idx>;
}


} // golos
