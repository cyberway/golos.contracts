#pragma once
#include "parameters.hpp"
#include "objects.hpp"

namespace golos {


using namespace eosio;

class vesting : public contract {

public:
    using contract::contract;

    [[eosio::action]] void validateprms(symbol symbol, std::vector<vesting_params>);
    [[eosio::action]] void setparams(symbol symbol, std::vector<vesting_params>);

    [[eosio::action]] void retire(asset quantity, name user);
    [[eosio::action]] void unlocklimit(name owner, asset quantity);

    [[eosio::action]] void convert(name from, name to, asset quantity);
    [[eosio::action]] void cancelconv(name sender, symbol type);
    [[eosio::action]] void delegate(name sender, name recipient, asset quantity, uint16_t interest_rate, uint8_t payout_strategy);
    [[eosio::action]] void undelegate(name sender, name recipient, asset quantity);

    [[eosio::action]] void create(symbol symbol, name notify_acc);

    [[eosio::action]] void open(name owner, symbol symbol, name ram_payer);
    [[eosio::action]] void close(name owner, symbol symbol);

    [[eosio::action]] void timeout();
    [[eosio::action]] void timeoutconv();
    [[eosio::action]] void timeoutrdel();
    [[eosio::action]] void paydelegator(name account, asset reward, name delegator, uint8_t payout_strategy);

    void on_transfer(name from, name to, asset quantity, std::string memo);

    static inline asset get_account_vesting(name code, name account, symbol_code sym) {
        tables::account_table accounts(code, account.value);
        const auto& account_obj = accounts.get(sym.raw());
        return account_obj.vesting;
    }
    static inline asset get_account_effective_vesting(name code, name account, symbol_code sym) {
        tables::account_table accounts(code, account.value);
        const auto& account_obj = accounts.get(sym.raw());
        return account_obj.effective_vesting();
    }
    static inline asset get_account_available_vesting(name code, name account, symbol_code sym) {
        tables::account_table accounts(code, account.value);
        const auto& account_obj = accounts.get(sym.raw());
        return account_obj.available_vesting();
    }
    static inline asset get_account_unlocked_vesting(name code, name account, symbol_code sym) {
        tables::account_table accounts(code, account.value);
        const auto& account_obj = accounts.get(sym.raw());
        return account_obj.unlocked_vesting();
    }
    static inline bool balance_exist(name code, name owner, symbol_code sym) {
        tables::account_table accounts(code, owner.value);
        return accounts.find(sym.raw()) != accounts.end();
    }

    static inline std::vector<structures::delegate_record> 
    get_list_delegate(name code, name owner, symbol_code sym) {
        std::vector<structures::delegate_record> list_result;
        tables::delegate_table delegates(code, sym.raw());
        auto index_delegate = delegates.get_index<"recipient"_n>();
        
        auto record_index = index_delegate.find(owner.value);
        while (record_index != index_delegate.end() && record_index->recipient == owner) {
            list_result.push_back(*record_index);
            ++record_index;
        }

        return list_result;
    }

private:
    void notify_balance_change(name owner, asset diff);
    void sub_balance(name owner, asset value, bool retire_mode = false);
    void add_balance(name owner, asset value, name ram_payer);
    void send_account_event(name account, const structures::user_balance& balance);
    void send_vesting_event(const structures::vesting_info& info);

    const asset convert_to_token(const asset& vesting, const structures::vesting_info& vinfo) const;
    const asset convert_to_vesting(const asset& token, const structures::vesting_info& vinfo) const;

    static name get_recipient(const std::string& memo);
};

} // golos
