/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once
#include <eosio/eosio.hpp>
#include "types.h"
#include <common/calclib/atmsp_storable.h>
#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <string>
#include <common/config.hpp>
#include <golos.vesting/golos.vesting.hpp>

// disable for now. will be enabled later when properly tested
#define DISABLE_CHARGE_VESTING
// disable for now. not used
#define DISABLE_CHARGE_STORABLE


namespace golos {
using namespace eosio;

class [[eosio::contract("golos.charge")]] charge : public contract {
    fixp_t consume_charge(name issuer, name user, symbol_code token_code, uint8_t charge_id, int64_t price, int64_t cutoff = -1, int64_t vesting_price = -1);
    static inline fixp_t to_fixp(int64_t arg)   { return fp_cast<fixp_t>(arg); }
    static inline int64_t from_fixp(fixp_t arg) { return fp_cast<int64_t>( arg, false ); }

public:
    using contract::contract;
    static inline int64_t get_stored(name code, name user, symbol_code token_code, uint8_t charge_id, uint64_t stamp_id) {
        storedvals storedvals_table(code, user.value);
        auto storedvals_index = storedvals_table.get_index<"symbolstamp"_n>();
        auto itr = storedvals_index.find(stored::get_key(token_code, charge_id, stamp_id));
        return itr != storedvals_index.end() ? from_fixp(FP(itr->value)) : -1; //charge can't be negative
    }

    static inline bool exist(name code, symbol_code token_code, uint8_t charge_id) {
        restorers restorers_table(code, code.value);
        return restorers_table.find(symbol(token_code, charge_id).raw()) != restorers_table.end();
    }
    [[eosio::action]] void use(name user, symbol_code token_code, uint8_t charge_id, int64_t price, int64_t cutoff, int64_t vesting_price);
    [[eosio::action]] void usenotifygt(name user, symbol_code token_code, uint8_t charge_id, int64_t price_arg, int64_t id, name code, name action_name, int64_t cutoff);
    [[eosio::action]] void usenotifylt(name user, symbol_code token_code, uint8_t charge_id, int64_t price_arg, int64_t id, name code, name action_name, int64_t cutoff);

#ifndef DISABLE_CHARGE_STORABLE
    [[eosio::action]] void removestored(name user, symbol_code token_code, uint8_t charge_id, int64_t stamp_id);
    [[eosio::action]] void useandstore(name user, symbol_code token_code, uint8_t charge_id, int64_t stamp_id, int64_t price_arg);
#endif

    [[eosio::action]] void setrestorer(symbol_code token_code, uint8_t charge_id, std::string func_str,
        int64_t max_prev, int64_t max_vesting, int64_t max_elapsed);

    void on_transfer(name from, name to, asset quantity, std::string memo);
    //TODO:? user can restore a charge by transferring some amount to this contract (it will send these funds to the issuer)
    //a price is specified in a settings, charge_id is in a memo (default charge_id for empty memo)

private:
    template<typename Lambda>
    void consume_and_notify(name user, symbol_code token_code, uint8_t charge_id, int64_t price_arg, int64_t id,
        name code, name action_name, int64_t cutoff, Lambda&& compare);

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

    using balances [[eosio::order("charge_symbol","asc")]] = eosio::multi_index<"balances"_n, balance>;
    using restorers [[eosio::order("charge_symbol","asc")]] = eosio::multi_index<"restorers"_n, restorer>;
    using stored_key_idx [[eosio::order("symbol_stamp","asc")]] = indexed_by<"symbolstamp"_n, const_mem_fun<stored, uint128_t, &stored::key> >;
    using storedvals [[eosio::order("id","asc")]] = eosio::multi_index<"storedvals"_n, stored, stored_key_idx>;


    static inline fixp_t calc_value(name code, name user, symbol_code token_code, const balance& user_balance, fixp_t price) {
        auto cur_time = eosio::current_time_point().time_since_epoch().count();
        eosio::check(cur_time >= user_balance.last_update, "LOGIC ERROR! charge::calc_value: cur_time < user_balance.last_update");

        auto prev = FP(user_balance.value);
        fixp_t restored(0);
        if (cur_time > user_balance.last_update) {
            restorers tbl(code, code.value);
            auto itr = tbl.get(user_balance.charge_symbol, "charge::calc_value restorer itr == tbl.end()");
            atmsp::machine<fixp_t> machine;

            int64_t elapsed_seconds = static_cast<int64_t>((cur_time - user_balance.last_update) / eosio::seconds(1).count());
#           ifdef DISABLE_CHARGE_VESTING
                int64_t vesting = 0;
#           else
                int64_t vesting = golos::vesting::get_account_effective_vesting(config::vesting_name, user, token_code).amount;
#           endif
            itr.func.to_machine(machine);
            restored = machine.run(
                {prev, fp_cast<fixp_t>(vesting, false), fp_cast<fixp_t>(elapsed_seconds, false)}, {
                    {fixp_t(0), FP(itr.max_prev)},
                    {fixp_t(0), FP(itr.max_vesting)},
                    {fixp_t(0), FP(itr.max_elapsed)}
                });
            eosio::check(restored >= fixp_t(0), "charge::calc_value restored < 0");
        }
        return std::max(prev - restored + price, fixp_t(0));
    }

private:
    struct [[eosio::event]] chargestate {
        name user;
        uint64_t charge_symbol;
        symbol_code token_code;
        uint8_t charge_id;
        uint64_t last_update;
        base_t value;
    };
    void send_charge_event(name user, const balance& state);

public:
    static inline int64_t get_current_value(name code, name user, symbol_code token_code, uint8_t charge_id = 0) {
        balances balances_table(code, user.value);
        balances::const_iterator itr = balances_table.find(symbol(token_code, charge_id).raw());
        return (itr != balances_table.end()) ? from_fixp(calc_value(code, user, token_code, *itr, fixp_t(0))) : 0;
    }
};

} /// namespace eosio
