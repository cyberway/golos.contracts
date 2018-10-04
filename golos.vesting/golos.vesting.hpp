#pragma once
#include "objects.hpp"

namespace eosio {

class vesting : public eosio::contract {

  public:
    vesting(account_name self) : contract(self)  {}
    void apply(uint64_t code, uint64_t action);

    void buy_vesting(account_name from, account_name  to, asset quantity, std::string);
    void accrue_vesting(account_name sender, account_name user, asset quantity);
    void convert_vesting(account_name sender, account_name recipient, asset quantity);
    void cancel_convert_vesting(account_name sender, asset type);
    void delegate_vesting(account_name sender, account_name recipient, asset quantity, uint16_t percentage_deductions);
    void undelegate_vesting(account_name sender, account_name recipient, asset quantity);

    void create_token_vesting(symbol_type token_name);

    void open(account_name owner, symbol_type symbol, account_name ram_payer );
    void close( account_name owner, symbol_type symbol );

    inline asset get_account_vesting(account_name account, symbol_type sym )const;

  private:
    void sub_balance(account_name owner, asset value);
    void add_balance(account_name owner, asset value, account_name ram_payer);
    const bool bool_asset(const asset &obj) const;

    const asset convert_to_token(const asset &m_token, const structures::supply &supply) const;
    const asset convert_to_vesting(const asset &m_token, const structures::supply &supply) const;

    void timeout_delay_trx();
    void calculate_convert_vesting();
    void calculate_delegate_vesting();
};

asset vesting::get_account_vesting(account_name account, symbol_type sym)const {
    tables::account_table balances(_self, account);
    auto balance = balances.find(sym.name());
    if (balance != balances.end())
        return balance->vesting;

    return asset(0, sym);
}

}

