/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>

#include <string>

namespace eosiosystem {
   class system_contract;
}

namespace eosio {

   using std::string;

   class deposit : public contract {
      public:
         deposit( account_name self ):contract(self){}

         /// @abi action
         void transfer( account_name from,
                        account_name to,
                        asset        quantity,
                        string       memo );

         inline asset get_balance (account_name owner, symbol_name sym) const;

         void on_transfer( account_name from,
                           account_name to,
                           asset        quantity,
                           string       memo );

      private:
         struct balance {
            asset quantity;

            uint64_t primary_key() const {return quantity.symbol.name();}
            EOSLIB_SERIALIZE(balance, (quantity))
         };

         typedef eosio::multi_index<N(balance), balance> balance_table;

         void sub_balance(account_name owner, asset value);
         void add_balance(account_name owner, asset value, account_name ram_payer);
   };

   asset deposit::get_balance(account_name owner, symbol_name sym) const
   {
      balance_table table( _self, owner );
      auto iter = table.find(sym);
      if (iter == table.end()) {
          return asset(0,symbol_type(S(0,symbol_name)));
      } else {
          return iter->quantity;
      }
   }

} /// namespace eosio
