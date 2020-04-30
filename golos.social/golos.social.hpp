#pragma once
#include "objects.hpp"

namespace golos {

using namespace eosio;

class [[eosio::contract("golos.social")]] social : public contract {
private:
    using contract::contract;
public:
    [[eosio::action]] void pin(name pinner, name pinning);
    [[eosio::action]] void addpin(name pinner, name pinning);
    [[eosio::action]] void unpin(name pinner, name pinning);

    [[eosio::action]] void block(name blocker, name blocking);
    [[eosio::action]] void addblock(name blocker, name blocking);
    [[eosio::action]] void unblock(name blocker, name blocking);

    [[eosio::action]] void updatemeta(name account, accountmeta meta);
    [[eosio::action]] void deletemeta(name account);

     static inline bool is_blocking(name code, name blocker, name blocking) {
         tables::pinblock_table table(code, blocker.value);
         auto itr = table.find(blocking.value);
         return (itr != table.end() && itr->blocking);
     }
};

} // golos
