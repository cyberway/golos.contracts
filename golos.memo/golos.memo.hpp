#pragma once
#include <eosio/eosio.hpp>
#include <eosio/crypto.hpp>

namespace golos {

using namespace eosio;


// class [[eosio::contract("golos.memo")]] memo: public contract {
class memo {

    struct [[eosio::table("memo")]] memo_key {
        name name;
        public_key key;

        uint64_t primary_key() const {
            return name.value;
        }
    };
    using memo_tbl = eosio::multi_index<"memo"_n, memo_key>;

public:
    // using contract::contract;

    static inline bool has_memo(name code, name account) {
        memo_tbl tbl(code, code.value);
        auto itr = tbl.find(account.value);
        return itr != tbl.end();
    }

    static inline public_key get_memo(name code, name account) {
        memo_tbl tbl(code, code.value);
        return tbl.get(account.value).key;
    }
};

} // golos
