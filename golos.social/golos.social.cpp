#include "golos.social.hpp"

namespace golos {

using namespace eosio;

EOSIO_DISPATCH(social, (changereput)(pin)(unpin)(block)(unblock))

void social::changereput(name voter, name author, int64_t rshares) {
    require_auth(_self);

    tables::reputation_singleton voter_single(_self, voter.value);
    auto voter_rep = voter_single.get_or_default();

    tables::reputation_singleton author_single(_self, author.value);
    auto author_rep = author_single.get_or_create(author);

    // Rule #1: Must have non-negative reputation to affect another user's reputation
    if (voter_rep.reputation < 0) {
        return;
    }

    // Rule #2: If you are downvoяting another user, you must have more reputation than him
    if (rshares < 0 && voter_rep.reputation <= author_rep.reputation) {
        return;
    }

    author_rep.reputation += rshares;
    author_single.set(author_rep, author);
}

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
