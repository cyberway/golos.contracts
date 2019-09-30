#include <eosio/event.hpp>
#include "golos.social.hpp"

namespace golos {

using namespace eosio;

EOSIO_DISPATCH(social, (pin)(unpin)(block)(unblock)(updatemeta)(deletemeta))

void social::pin(name pinner, name pinning) {
    auto user_auth = has_auth(pinner);
    eosio::check(user_auth || has_auth(_self), "missing required signature");
    if (!user_auth) {
        eosio::check(is_account(pinner), "Pinner account doesn't exist.");
    }

    eosio::check(is_account(pinning), "Pinning account doesn't exist.");
    eosio::check(pinner != pinning, "You cannot pin yourself");

    tables::pinblock_table table(_self, pinner.value);
    auto itr = table.find(pinning.value);
    bool item_exists = (itr != table.end());

    if (item_exists) {
        eosio::check(!itr->blocking, "You have blocked this account. Unblock it before pinning");
        eosio::check(!itr->pinning, "You already have pinned this account");

        table.modify(itr, name(), [&](auto& item){
            item.pinning = true;
        });

    } else {
        table.emplace(user_auth ? pinner : _self, [&](auto& item){
            item.account = pinning;
            item.pinning = true;
        });
    }
}

void social::unpin(name pinner, name pinning) {
    auto user_auth = has_auth(pinner);
    eosio::check(user_auth || has_auth(_self), "missing required signature");
    if (!user_auth) {
        eosio::check(is_account(pinner), "Pinner account doesn't exist.");
    }

    eosio::check(pinner != pinning, "You cannot unpin yourself");

    tables::pinblock_table table(_self, pinner.value);
    auto itr = table.find(pinning.value);
    bool item_exists = (itr != table.end());

    eosio::check(item_exists && itr->pinning, "You have not pinned this account");

    table.modify(itr, name(), [&](auto& item){
        item.pinning = false;
    });

    if (record_is_empty(*itr))
        table.erase(itr);
}

bool social::record_is_empty(structures::pinblock_record record) {
    return !record.pinning && !record.blocking;
}

void social::block(name blocker, name blocking) {
    auto user_auth = has_auth(blocker);
    eosio::check(user_auth || has_auth(_self), "missing required signature");
    if (!user_auth) {
        eosio::check(is_account(blocker), "Blocker account doesn't exist.");
    }

    eosio::check(is_account(blocking), "Blocking account doesn't exist.");
    eosio::check(blocker != blocking, "You cannot block yourself");

    tables::pinblock_table table(_self, blocker.value);
    auto itr = table.find(blocking.value);
    bool item_exists = (itr != table.end());

    if (item_exists) {
        eosio::check(!itr->blocking, "You already have blocked this account");

        table.modify(itr, name(), [&](auto& item){
            item.pinning = false;
            item.blocking = true;
        });

    } else {
        table.emplace(user_auth ? blocker : _self, [&](auto& item){
            item.account = blocking;
            item.blocking = true;
        });
    }
}

void social::unblock(name blocker, name blocking) {
    auto user_auth = has_auth(blocker);
    eosio::check(user_auth || has_auth(_self), "missing required signature");
    if (!user_auth) {
        eosio::check(is_account(blocker), "Blocker account doesn't exist.");
    }

    eosio::check(blocker != blocking, "You cannot unblock yourself");

    tables::pinblock_table table(_self, blocker.value);
    auto itr = table.find(blocking.value);
    bool item_exists = (itr != table.end());

    eosio::check(item_exists && itr->blocking, "You have not blocked this account");

    table.modify(itr, name(), [&](auto& item){
        item.blocking = false;
    });

    if (record_is_empty(*itr))
        table.erase(itr);
}

void social::updatemeta(name account, accountmeta meta) {
    require_auth(account);
}

void social::deletemeta(name account) {
    require_auth(account);
}


} // golos
