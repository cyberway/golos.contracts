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

namespace golos {
using namespace eosio;

class charge : public contract {
    
public:
    static symbol_name get_name(symbol_name token_name, unsigned char suffix = 0) { //0 for default, vesting check default suffix
        return (token_name << 8) | static_cast<uint64_t>(suffix);
        //token and vesting use one byte to store precision, and it is not used here, 
        //so this byte can be used for a name/number of a charge. 
        //thus, each token can have up to 256 charges. 
        //for example, 'p' - for posting, 'c' - for commenting, 'b' - for post_band etc
    }
    charge( account_name self ):contract(self){}
    void apply(uint64_t code, uint64_t action);
    fixp_t get(account_name user, symbol_name token_name, unsigned char suffix, fixp_t price_base) const;
    //TODO:? in a case when you want to know a value of the charge from outside
    //(actually, for now it only happens with post_band calculations), there are issues:
    //* a value may be outdated: inline action is performed after the current one;
    //* new value is calculated twice - here and in "use" action.

private:
    void use(account_name user, symbol_name token_name, unsigned char suffix, uint64_t price_base, uint64_t cutoff_base);
    
    void set_restorer(symbol_name token_name, unsigned char suffix, std::string func_str, 
        uint64_t max_prev, uint64_t max_vesting, uint64_t max_elapsed);
    
    struct balance {
        symbol_name charge_name;
        uint64_t last_update;
        base_t value;
        uint64_t primary_key()const { return charge_name; }
    };
    
    struct restorer {
        symbol_name charge_name;
        atmsp::storable::bytecode func;
        
        base_t max_prev;
        base_t max_vesting;
        base_t max_elapsed;
        
        uint64_t primary_key()const { return charge_name; }
    };

    typedef eosio::multi_index<N(balances), balance> balances;
    typedef eosio::multi_index<N(restorers), restorer> restorers;
    
    fixp_t calc_value(account_name user, symbol_name token_name, balances& balances_table, balances::const_iterator& itr, fixp_t price) const;

    void on_transfer(account_name from, account_name to, asset quantity, std::string memo);
    //TODO:? user can restore a charge by transferring some amount to this contract (it will send these funds to the issuer)
    //a price is specified in a settings, the suffix is in a memo (default suffix for empty memo)
};

fixp_t charge::get(account_name user, symbol_name token_name, unsigned char suffix, fixp_t price) const {
    eosio_assert(price >= 0, "price can't be negative");
    balances balances_table(_self, user);
    auto itr = balances_table.find(get_name(token_name, suffix));
    return calc_value(user, token_name, balances_table, itr, price);
}

} /// namespace eosio
