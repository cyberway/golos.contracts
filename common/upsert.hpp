#pragma once

#include <eosiolib/eosio.hpp>

namespace golos {


// TODO: can be simplified to <class T>: F depends only on T's item type, which possible can be derived from T
template<typename T, typename F>
bool upsert_tbl(uint64_t code, uint64_t scope, account_name payer, uint64_t key, F&& get_update_fn, bool allow_insert = true) {
    T tbl{code, scope};
    auto itr = tbl.find(key);
    bool exists = itr != tbl.end();
    if (exists) {
        tbl.modify(itr, payer, get_update_fn(exists));
    } else if (allow_insert) {
        tbl.emplace(payer, get_update_fn(exists));
    }
    return exists;
}


} // namespace golos
