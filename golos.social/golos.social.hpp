#pragma once
#include "objects.hpp"

namespace golos {

using namespace eosio;

class social : public contract {
public:
    social(account_name self);
    void apply(uint64_t code, uint64_t action);

private:
    void pin(account_name pinner, account_name pinning);
    void unpin(account_name pinner, account_name pinning);
    void block(account_name blocker, account_name blocking);
    void unblock(account_name blocker, account_name blocking);
};

} // golos
