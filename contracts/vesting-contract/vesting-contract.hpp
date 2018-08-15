#pragma once
#include "objects.hpp"

namespace eosio {

class vesting : public eosio::contract {
  public:
    vesting(account_name self);

    void buy_vesting(const structures::transfer_vesting &m_transfer_vesting);
    void accrue_vesting(const structures::accrue_vesting &m_accrue_vesting);
    void convert_vesting(const structures::transfer_vesting &m_transfer_vesting);
    void delegate_vesting(const structures::delegate &m_delegate_vesting);
    void undelegate_vesting(const structures::transfer_vesting &m_transfer_vesting);
};

}

