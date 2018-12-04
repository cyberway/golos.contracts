/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once
#include <eosiolib/eosio.hpp>
#include "types.h"
#include <common/calclib/atmsp_storable.h>
#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>
#include <string>
#include "config.hpp"

namespace golos {
using namespace eosio;

class charge : public contract {
    
public:
    using contract::contract;
    fixp_t get(name user, symbol_code token_code, unsigned char suffix, fixp_t price_base) const;
    //TODO:? in a case when you want to know a value of the charge from outside
    //(actually, for now it only happens with post_band calculations), there are issues:
    //* a value may be outdated: inline action is performed after the current one;
    //* new value is calculated twice - here and in "use" action.

    void use(name user, symbol_code token_code, unsigned char suffix, uint64_t price_base, uint64_t cutoff_base);
    
    void set_restorer(symbol_code token_code, unsigned char suffix, std::string func_str, 
        uint64_t max_prev, uint64_t max_vesting, uint64_t max_elapsed);
        
    void on_transfer(name from, name to, asset quantity, std::string memo);
    //TODO:? user can restore a charge by transferring some amount to this contract (it will send these funds to the issuer)
    //a price is specified in a settings, the suffix is in a memo (default suffix for empty memo)

private:
    struct balance {
        uint64_t charge_symbol;
        uint64_t last_update;
        base_t value;
        uint64_t primary_key()const { return charge_symbol; }
    };
    
    struct restorer {
        uint64_t charge_symbol;
        atmsp::storable::bytecode func;
        
        base_t max_prev;
        base_t max_vesting;
        base_t max_elapsed;
        
        uint64_t primary_key()const { return charge_symbol; }
    };

    typedef eosio::multi_index<N(balances), balance> balances;
    typedef eosio::multi_index<N(restorers), restorer> restorers;
    
    fixp_t calc_value(name user, symbol_code token_code, balances& balances_table, balances::const_iterator& itr, fixp_t price) const;

};

fixp_t charge::get(name user, symbol_code token_code, unsigned char suffix, fixp_t price) const {
    eosio_assert(price >= 0, "price can't be negative");
    balances balances_table(_self, user.value);
    auto itr = balances_table.find(symbol(token_code, suffix).raw());
    return calc_value(user, token_code, balances_table, itr, price);
}

} /// namespace eosio
