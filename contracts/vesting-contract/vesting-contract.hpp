#pragma once
#include "objects.hpp"

namespace eosio {

class vesting : public eosio::contract {
  public:
    vesting(account_name self);
    void apply(uint64_t code, uint64_t action);

    void buy_vesting(const structures::transfer_vesting &m_transfer_vesting);
    void accrue_vesting(const structures::accrue_vesting &m_accrue_vesting);
    void convert_vesting(const structures::transfer_vesting &m_transfer_vesting);
    void delegate_vesting(const structures::delegate_vesting &m_delegate_vesting);
    void undelegate_vesting(const structures::transfer_vesting &m_transfer_vesting);

    void init_contract();
    void calculate_convert_vesting();
    void calculate_delegate_vesting();

  private:
    void add_balance(account_name owner, asset value, account_name ram_payer);
    void sub_balance(account_name owner, asset value);

    asset convert_token_to_vesting(const asset &m_token);

  private:
    tables::convert_table _table_convert;
    tables::return_delegate_table _table_delegate_vesting;
};

}

