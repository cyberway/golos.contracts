#include <eosio/event.hpp>
#include "golos.social.hpp"

namespace golos {

using namespace eosio;

void social::pin(name pinner, name pinning) {
    require_auth(pinner);

    eosio::check(is_account(pinning), "Pinning account doesn't exist.");
    eosio::check(pinner != pinning, "You cannot pin yourself");

    tables::pinblock_table table(_self, pinner.value);
    auto itr = table.find(pinning.value);
    if (itr != table.end()) {
        eosio::check(!itr->blocking, "You have blocked this account. Unblock it before pinning");
        eosio::check(!itr->pinning, "You already have pinned this account");
        eosio::check(false, "SYSTEM: account not blocked and not pinned, but record exists - error in data");
    } else {
        table.emplace(pinner, [&](auto& item){
            item.account = pinning;
            item.pinning = true;
        });
    }
}

void social::addpin(name pinner, name pinning) {
    require_auth(_self);

    eosio::check(is_account(pinner), "Pinner account doesn't exist.");
    eosio::check(is_account(pinning), "Pinning account doesn't exist.");
    eosio::check(pinner != pinning, "Pinner cannot pin himself");

    tables::pinblock_table table(_self, pinner.value);
    auto itr = table.find(pinning.value);
    eosio::check(itr == table.end(), "Record already exists.");

    table.emplace(_self, [&](auto& item){
        item.account = pinning;
        item.pinning = true;
    });
}

void social::unpin(name pinner, name pinning) {
    require_auth(pinner);

    eosio::check(is_account(pinning), "Pinning account doesn't exist.");
    eosio::check(pinner != pinning, "You cannot unpin yourself");

    tables::pinblock_table table(_self, pinner.value);
    auto itr = table.find(pinning.value);
    if (itr != table.end()) {
        eosio::check(itr->pinning, "You have not pinned this account");
        table.erase(itr);
    }
}

void social::block(name blocker, name blocking) {
    require_auth(blocker);

    eosio::check(is_account(blocking), "Blocking account doesn't exist.");
    eosio::check(blocker != blocking, "You cannot block yourself");

    tables::pinblock_table table(_self, blocker.value);
    auto itr = table.find(blocking.value);
    if (itr != table.end()) {
        eosio::check(!itr->blocking, "You already have blocked this account");
        eosio::check(itr->pinning, "SYSTEM: account not blocked and not pinned, but record exists - error in data");

        table.modify(itr, name(), [&](auto& item){
            item.pinning = false;
            item.blocking = true;
        });
    } else {
        table.emplace(blocker, [&](auto& item){
            item.account = blocking;
            item.blocking = true;
        });
    }
}

void social::addblock(name blocker, name blocking) {
    require_auth(_self);

    eosio::check(is_account(blocker), "Blocker account doesn't exist.");
    eosio::check(is_account(blocking), "Blocking account doesn't exist.");
    eosio::check(blocker != blocking, "Blocker cannot block himself");

    tables::pinblock_table table(_self, blocker.value);
    auto itr = table.find(blocking.value);
    eosio::check(itr == table.end(), "Record already exists.");

    table.emplace(_self, [&](auto& item){
        item.account = blocking;
        item.blocking = true;
    });
}

void social::unblock(name blocker, name blocking) {
    require_auth(blocker);

    eosio::check(is_account(blocking), "Blocking account doesn't exist.");
    eosio::check(blocker != blocking, "You cannot unblock yourself");

    tables::pinblock_table table(_self, blocker.value);
    auto itr = table.find(blocking.value);
    if (itr != table.end()) {
        eosio::check(itr->blocking, "You have not blocked this account");
        table.erase(itr);
    }
}

void social::updatemeta(name account, accountmeta meta) {
    require_auth(account);
}

void social::deletemeta(name account) {
    require_auth(account);
}


} // golos
