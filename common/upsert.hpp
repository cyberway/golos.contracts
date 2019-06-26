#pragma once
#include <eosio/eosio.hpp>

namespace golos {


// TODO: can be simplified to <class T>: F depends only on T's item type, which possible can be derived from T
template<typename T, typename F>
bool upsert_tbl(eosio::name code, uint64_t scope, eosio::name payer, uint64_t key, F&& get_update_fn, bool allow_insert = true) {
    T tbl{code, scope};
    auto itr = tbl.find(key);
    bool exists = itr != tbl.end();
    if (exists || allow_insert) {
        auto update_fn = get_update_fn(exists);
        if (exists)
            tbl.modify(itr, payer, update_fn);
        else
            tbl.emplace(payer, update_fn);
    }
    return exists;
}


} // golos
