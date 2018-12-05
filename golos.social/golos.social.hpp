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
};

} // golos
