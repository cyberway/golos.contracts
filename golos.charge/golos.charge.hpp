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
    fixp_t use_helper(name issuer, name user, symbol_code token_code, uint8_t charge_id, int64_t price, int64_t cutoff = -1, int64_t vesting_price = -1);
    static inline fixp_t to_fixp(int64_t arg) { return fp_cast<fixp_t>(elai_t(arg) / elai_t(config::_100percent)); }
    static inline int64_t from_fixp(fixp_t arg) { return fp_cast<int64_t>(elap_t(arg) * elai_t(config::_100percent), false); }
public:
    using contract::contract;    
    static inline int64_t get(name code, name user, symbol_code token_code, uint8_t charge_id, uint64_t stamp_id) {
        storedvals storedvals_table(code, user.value);
        auto storedvals_index = storedvals_table.get_index<N(symbolstamp)>();
        auto itr = storedvals_index.find(stored::get_key(token_code, charge_id, stamp_id));
        return itr != storedvals_index.end() ? from_fixp(FP(itr->value)) : -1; //charge can't be negative
    }

    [[eosio::action]] void use(name user, symbol_code token_code, uint8_t charge_id, int64_t price, int64_t cutoff, int64_t vesting_price);
    [[eosio::action]] void useandstore(name user, symbol_code token_code, uint8_t charge_id, int64_t stamp_id, int64_t price);
    [[eosio::action]] void removestored(name user, symbol_code token_code, uint8_t charge_id, int64_t stamp_id);
    
    [[eosio::action]] void setrestorer(symbol_code token_code, uint8_t charge_id, std::string func_str, 
        int64_t max_prev, int64_t max_vesting, int64_t max_elapsed);
        
    void on_transfer(name from, name to, asset quantity, std::string memo);
    //TODO:? user can restore a charge by transferring some amount to this contract (it will send these funds to the issuer)
    //a price is specified in a settings, charge_id is in a memo (default charge_id for empty memo)

private:
    struct balance {
        uint64_t charge_symbol;
        symbol_code token_code;
        uint8_t charge_id;
        uint64_t last_update;
        base_t value;
        uint64_t primary_key()const { return charge_symbol; }
    };
    
    struct restorer {
        uint64_t charge_symbol;
        symbol_code token_code;
        uint8_t charge_id;
        atmsp::storable::bytecode func;
        
        base_t max_prev;
        base_t max_vesting;
        base_t max_elapsed;
        
        uint64_t primary_key()const { return charge_symbol; }
    };
    
    struct stored {
        static uint128_t get_key(symbol_code token_code, uint8_t charge_id, uint64_t stamp_id) {
            return (static_cast<uint128_t>(symbol(token_code, charge_id).raw()) << 64) | stamp_id;
        }
        uint64_t id;
        uint128_t symbol_stamp;
        base_t value;
        uint64_t primary_key() const {
            return id;
        }
        uint128_t key()const {
            return symbol_stamp;
        }
    };

    using balances = eosio::multi_index<N(balances), balance>;
    using restorers = eosio::multi_index<N(restorers), restorer>;
    using stored_key_idx = indexed_by<N(symbolstamp), const_mem_fun<stored, uint128_t, &stored::key> >;
    using storedvals = eosio::multi_index<N(storedvals), stored, stored_key_idx>;
    
    fixp_t calc_value(name user, symbol_code token_code, balances& balances_table, balances::const_iterator& itr, fixp_t price) const;

};

} /// namespace eosio
