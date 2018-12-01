#pragma once
#include "objects.hpp"

namespace golos {


using namespace eosio;

class vesting : public contract {

public:
    vesting(account_name self) : contract(self) {}
    void apply(uint64_t code, uint64_t action);

    void on_transfer(account_name from, account_name to, asset quantity, std::string memo);
    void retire(asset quantity, account_name user);
    void unlock_limit(account_name owner, asset quantity);

    void convert_vesting(account_name from, account_name to, asset quantity);
    void cancel_convert_vesting(account_name sender, asset type);
    void delegate_vesting(account_name sender, account_name recipient, asset quantity, uint16_t interest_rate, uint8_t payout_strategy);
    void undelegate_vesting(account_name sender, account_name recipient, asset quantity);

    void create(symbol_type symbol);

    void open(account_name owner, symbol_type symbol, account_name ram_payer);
    void close(account_name owner, symbol_type symbol);

    inline asset get_account_vesting(account_name account, symbol_name sym) const;
    inline asset get_account_effective_vesting(account_name account, symbol_name sym) const;
    inline asset get_account_available_vesting(account_name account, symbol_name sym) const;
    inline asset get_account_unlocked_vesting(account_name account, symbol_name sym) const;
    inline bool balance_exist(account_name owner, symbol_name sym) const;

private:
    void notify_balance_change(account_name owner, asset diff);
    void sub_balance(account_name owner, asset value, bool retire_mode = false);
    void add_balance(account_name owner, asset value, account_name ram_payer);

    const asset convert_to_token(const asset& vesting, const structures::vesting_info& vinfo) const;
    const asset convert_to_vesting(const asset& token, const structures::vesting_info& vinfo) const;

    void timeout_delay_trx();
    void calculate_convert_vesting();
    void calculate_delegate_vesting();
    
    static account_name get_receiver(account_name from, const std::string& memo);
};

asset vesting::get_account_vesting(account_name account, symbol_name sym) const {
    tables::account_table balances(_self, account);
    const auto& balance = balances.get(sym);
    return balance.vesting;
}

asset vesting::get_account_available_vesting(account_name account, symbol_name sym) const {
    tables::account_table balances(_self, account);
    const auto& balance = balances.get(sym);
    return balance.available_vesting();
}

asset vesting::get_account_effective_vesting(account_name account, symbol_name sym) const {
    tables::account_table balances(_self, account);
    const auto& balance = balances.get(sym);
    return balance.effective_vesting();
}

asset vesting::get_account_unlocked_vesting(account_name account, symbol_name sym) const {
    tables::account_table balances(_self, account);
    const auto& balance = balances.get(sym);
    return balance.unlocked_vesting();
}

bool vesting::balance_exist(account_name owner, symbol_name sym) const {
    tables::account_table balances(_self, owner);
    return balances.find(sym) != balances.end();
}


} // golos
