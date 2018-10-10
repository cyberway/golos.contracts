/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#include "deposit.hpp"

namespace eosio {

void deposit::transfer( account_name from,
                        account_name to,
                        asset        quantity,
                        string       memo )
{
    //eosio_assert( from != to, "cannot transfer to self" );
    require_auth( _self );
    //eosio_assert( is_account( to ), "to account does not exist");

    //require_recipient( from );
    //require_recipient( to );

    //eosio_assert( quantity.is_valid(), "invalid quantity" );
    //eosio_assert( quantity.amount > 0, "must transfer positive quantity" );
    //eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );

    action(
        permission_level{_self, N(trx)},
        N(eosio.token), N(transfer),
        std::make_tuple(_self, to, quantity, memo)
    ).send();

    sub_balance(from, quantity );
}

void deposit::on_transfer(account_name from,
                          account_name to,
                          asset        quantity,
                          string       memo)
{
    (void)memo;
    if(to == _self) {
        add_balance(from, quantity, _self);
    }
}

void deposit::sub_balance( account_name owner, asset value ) {
    balance_table table(_self, owner);
    auto iter = table.find(value.symbol.name());
    
    eosio_assert(iter != table.end(), "no balance object found");
    eosio_assert(iter->quantity >= value, "overdrawn balance");

    if(iter->quantity == value) {
        table.erase(iter);
    } else {
        table.modify(iter, 0, [&](auto& b) {
            b.quantity -= value;
        });
    }
}

void deposit::add_balance( account_name owner, asset value, account_name ram_payer )
{
    balance_table table(_self, owner);
    auto iter = table.find(value.symbol.name());
    if(iter == table.end()) {
        table.emplace(ram_payer, [&](auto& b) {
            b.quantity = value;
        });
    } else {
        table.modify(iter, 0, [&](auto& b) {
            b.quantity += value;
        });
    }
}

} /// namespace eosio

// extend from EOSIO_ABI
#define EOSIO_ABI_EX( TYPE, MEMBERS ) \
extern "C" { \
   void apply( uint64_t receiver, uint64_t code, uint64_t action ) { \
      auto self = receiver; \
      if( action == N(onerror)) { \
         /* onerror is only valid if it is for the "eosio" code account and authorized by "eosio"'s "active permission */ \
         eosio_assert(code == N(eosio), "onerror action's are only valid from the \"eosio\" system account"); \
      } \
      if( code == self || action == N(onerror) ) { \
         TYPE thiscontract( self ); \
         switch( action ) { \
            EOSIO_API( TYPE, MEMBERS ) \
         } \
         /* does not allow destructor of thiscontract to run: eosio_exit(0); */ \
      } else if(code == N(eosio.token)) { \
         TYPE thiscontract(self); \
         switch(action) { \
            case N(transfer): eosio::execute_action(&thiscontract, &TYPE::on_transfer); \
        } \
      } \
   } \
}

EOSIO_ABI_EX( eosio::deposit, (transfer) )
