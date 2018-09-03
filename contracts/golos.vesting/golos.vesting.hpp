#pragma once
#include "objects.hpp"
#include "eosio.token/eosio.token.hpp"

namespace eosio {

class vesting : public eosio::contract {
  public:
    vesting(account_name self);
    void apply(uint64_t code, uint64_t action);

    void buy_vesting(account_name from, account_name  to, asset quantity, string);
    void accrue_vesting(account_name sender, account_name user, asset quantity);
    void convert_vesting(account_name sender, account_name recipient, asset quantity);
    void cancel_convert_vesting(account_name sender, asset type);
    void delegate_vesting(account_name sender, account_name recipient, asset quantity, uint16_t percentage_deductions);
    void undelegate_vesting(account_name sender, account_name recipient, asset quantity);

    void calculate_convert_vesting();
    void calculate_delegate_vesting();

    void create_pair(asset token, asset vesting);

    inline asset get_account_vesting(account_name account, symbol_type sym )const;

  private:
    void transfer(account_name from, account_name to, asset quantity, bool is_autorization = true);
    void issue(const structures::issue_vesting &m_issue, bool is_autorization = true, bool is_buy = false);

    void sub_balance(account_name owner, asset value);
    void add_balance(account_name owner, asset value, account_name ram_payer);
    const bool bool_asset(const asset &obj) const;

    asset convert_token(const asset &m_token, symbol_type type);
    void timeout_delay_trx();

  private:
    tables::vesting_table _table_pair;
    tables::return_delegate_table _table_delegate_vesting;
};

asset vesting::get_account_vesting(account_name account, symbol_type sym) const
{
    tables::account_table balances(_self, account);
    auto balance = balances.find(sym.name());
    if (balance != balances.end())
        return balance->vesting;

    return asset(0, sym);
}

}

