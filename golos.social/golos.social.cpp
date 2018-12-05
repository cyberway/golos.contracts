#include "golos.social.hpp"

namespace golos {

using namespace eosio;

EOSIO_DISPATCH(social, (pin)(unpin)(block)(unblock))

void social::pin(name pinner, name pinning) {
    require_auth(pinner);

    eosio_assert(pinner != pinning, "You cannot pin yourself");

    tables::pinblock_table table(_self, pinner.value);
    auto itr = table.find(pinning.value);
    bool item_exists = (itr != table.end());

    if (item_exists) {
        eosio_assert(!itr->blocking, "You have blocked this account. Unblock it before pinning");
        eosio_assert(!itr->pinning, "You already have pinned this account");

        table.modify(itr, name(), [&](auto& item){
            item.pinning = true;
        });

        return;
    }

    table.emplace(pinner, [&](auto& item){
        item.account = pinning;
        item.pinning = true;
    });
}

void social::unpin(name pinner, name pinning) {
    require_auth(pinner);

    eosio_assert(pinner != pinning, "You cannot unpin yourself");

    tables::pinblock_table table(_self, pinner.value);
    auto itr = table.find(pinning.value);
    bool item_exists = (itr != table.end());

    eosio_assert(item_exists && itr->pinning, "You have not pinned this account");

    table.modify(itr, name(), [&](auto& item){
        item.pinning = false;
    });
}

void social::block(name blocker, name blocking) {
    require_auth(blocker);

    eosio_assert(blocker != blocking, "You cannot block yourself");

    tables::pinblock_table table(_self, blocker.value);
    auto itr = table.find(blocking.value);
    bool item_exists = (itr != table.end());

    if (item_exists) {
        eosio_assert(!itr->blocking, "You already have blocked this account");

        table.modify(itr, name(), [&](auto& item){
            item.pinning = false;
            item.blocking = true;
        });

        return;
    }

    table.emplace(blocker, [&](auto& item){
        item.account = blocking;
        item.blocking = true;
    });
}

void social::unblock(name blocker, name blocking) {
    require_auth(blocker);

    eosio_assert(blocker != blocking, "You cannot unblock yourself");

    tables::pinblock_table table(_self, blocker.value);
    auto itr = table.find(blocking.value);
    bool item_exists = (itr != table.end());

    eosio_assert(item_exists && itr->blocking, "You have not blocked this account");

    table.modify(itr, name(), [&](auto& item){
        item.blocking = false;
    });
}


} // golos
