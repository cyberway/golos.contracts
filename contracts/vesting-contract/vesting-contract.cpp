#include "vesting-contract.hpp"

using namespace eosio;

vesting::vesting(account_name self)
    : contract(self)
{}

void vesting::buy_vesting(const structures::transfer_vesting &m_transfer_vesting) {

}

void vesting::accrue_vesting(const structures::accrue_vesting &m_accrue_vesting) {

}

void vesting::convert_vesting(const structures::transfer_vesting &m_transfer_vesting) {

}

void vesting::delegate_vesting(const structures::delegate &m_delegate_vesting) {

}

void vesting::undelegate_vesting(const structures::transfer_vesting &m_transfer_vesting) {

}


EOSIO_ABI( eosio::vesting, (buy_vesting)(accrue_vesting)(convert_vesting)(delegate_vesting)(undelegate_vesting) )
