#pragma once
#include "parameters.hpp"
#include "objects.hpp"

namespace golos {


using namespace eosio;

class vesting : public contract {

public:
    using contract::contract;

    void validateprms(std::vector<vesting_params>);
    void setparams(std::vector<vesting_params>);

    void on_transfer(name from, name to, asset quantity, std::string memo);
    void retire(asset quantity, name user);
    void unlock_limit(name owner, asset quantity);

    void convert_vesting(name from, name to, asset quantity);
    void cancel_convert_vesting(name sender, asset type);
    void delegate_vesting(name sender, name recipient, asset quantity, uint16_t interest_rate, uint8_t payout_strategy);
    void undelegate_vesting(name sender, name recipient, asset quantity);

    void create(symbol symbol, name notify_acc);

    void open(name owner, symbol symbol, name ram_payer);
    void close(name owner, symbol symbol);

    void timeout_delay_trx();
    void calculate_convert_vesting();
    void calculate_delegate_vesting();

    static inline asset get_account_vesting(name code, name account, symbol_code sym) {
        tables::account_table balances(code, account.value);
        const auto& balance = balances.get(sym.raw());
        return balance.vesting;
    };
    static inline asset get_account_effective_vesting(name code, name account, symbol_code sym) {
        tables::account_table balances(code, account.value);
        const auto& balance = balances.get(sym.raw());
        return balance.effective_vesting();
    };
    static inline asset get_account_available_vesting(name code, name account, symbol_code sym) {
        tables::account_table balances(code, account.value);
        const auto& balance = balances.get(sym.raw());
        return balance.available_vesting();
    };
    static inline asset get_account_unlocked_vesting(name code, name account, symbol_code sym) {
        tables::account_table balances(code, account.value);
        const auto& balance = balances.get(sym.raw());
        return balance.unlocked_vesting();
    };
    static inline bool balance_exist(name code, name owner, symbol_code sym) {
        tables::account_table balances(code, owner.value);
        return balances.find(sym.raw()) != balances.end();
    };

private:
    void notify_balance_change(name owner, asset diff);
    void sub_balance(name owner, asset value, bool retire_mode = false);
    void add_balance(name owner, asset value, name ram_payer);

    const asset convert_to_token(const asset& vesting, const structures::vesting_info& vinfo) const;
    const asset convert_to_vesting(const asset& token, const structures::vesting_info& vinfo) const;
    
    static name get_recipient(const std::string& memo);
};

} // golos
