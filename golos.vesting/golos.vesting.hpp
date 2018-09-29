#pragma once
#include "objects.hpp"
// #include "../eosio.token/eosio.token.hpp"

namespace golos {

class vesting : public eosio::contract {
public:
    vesting(account_name self): contract(self) {};
    inline asset get_account_vesting(account_name account, symbol_type sym) const;
};

asset vesting::get_account_vesting(account_name account, symbol_type sym) const {
    ::tables::account_table balances(_self, account);
    auto balance = balances.find(sym.name());
    if (balance != balances.end())
        return balance->vesting;

    return asset(0, sym);
}

}
