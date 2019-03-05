#pragma once
#include "objects.hpp"

namespace golos {

using namespace eosio;

class social : public contract {
private:
    using contract::contract;
public:
    void pin(name pinner, name pinning);
    void unpin(name pinner, name pinning);

    void block(name blocker, name blocking);
    void unblock(name blocker, name blocking);

    void createreput(name account);
    void changereput(name voter, name author, int64_t rshares);

    void updatemeta(name account, accountmeta meta);
    void deletemeta(name account);

    static inline bool is_blocking(name code, name blocker, name blocking) {
        tables::pinblock_table table(code, blocker.value);
        auto itr = table.find(blocking.value);
        return (itr != table.end() && itr->blocking);
    }

private:
    bool record_is_empty(structures::pinblock_record record);
};

} // golos
