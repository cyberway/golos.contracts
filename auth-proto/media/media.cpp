#include <eosiolib/eosio.hpp>
#include <eosiolib/transaction.hpp>
#include <eosiolib/symbol.hpp>
#include <eosiolib/asset.hpp>
#include <eosio.token/eosio.token.hpp>
using namespace eosio;

typedef name permlink_name;

class media : public eosio::contract {
public:
    using contract::contract;



    void onerror() {
        eosio::onerror error = eosio::onerror::from_current_action();
        //         print("media::onerror(", data.sender_id, ", ", data.sent_trx, ")");
    }



    void on_transfer(account_name from, account_name to, asset quantity, std::string memo) {
        (void)memo;
        if(to != _self)
            return;

        balances table(_self, from);
        auto index = table.get_index<N(bysymbol)>();
        auto item = index.find(quantity.symbol.name());
        if(item == index.end()) {
            table.emplace(_self, [&](auto& b) {
                b.id = table.available_primary_key();
                b.quantity = quantity;
            });
        } else {
            index.modify(item, 0, [&](auto& b) {
                b.quantity += quantity;
            });
        }
    }



    /// @abi action
    // Post publishing
    void publish(account_name user, permlink_name permlink) {
        require_auth(user);

        postrewards rewards(_self, user);
        static const uint32_t delay = 30;

        auto reward = rewards.find(permlink);
        eosio_assert(reward == rewards.end(), "such post reward already exists");

        // Place info about post rewards into database
        bool need_schedule = rewards.begin() == rewards.end();
        rewards.emplace(_self, [&](auto& a) {
            a.permlink = permlink;
            a.reward_time = now() + delay;
            a.votes = 0;
        });

        // Schedule deferred transaction with rewards for post
        if(need_schedule) {
            transaction out{};
            out.actions.emplace_back(
                    permission_level{user, N(active)}, 
                    _self, N(reward), 
                    std::make_tuple(user));
            out.delay_sec = delay;
            out.send(user, _self);
        }
    }



    /// @abi action
    // Voting for a post with a waste of battery power
    void vote(account_name user, account_name author, permlink_name permlink) {
        require_auth(user);

        postrewards rewards(_self, author);
        auto reward = rewards.find(permlink);

        eosio_assert(reward != rewards.end(), "No such post reward");
        rewards.modify(reward, _self, [&](auto& a) {
            a.votes++;
        });

        modify_data([&](auto& o) {
            o.total_votes++;
        });
    }



    /// @abi action
    // Voting for a post with a waste of vesting from user-account
    void vote2(account_name user, account_name author, permlink_name permlink) {
        auto value = asset(1, symbol_type(S(4,SYS)));
        action(
            permission_level{user, N(trx)}, 
            N(eosio.token), N(transfer), 
            std::make_tuple(
                user, N(eosio.null),
                value,
                std::string("Payment for voting ")+name{author}.to_string()+"@"+permlink.to_string())
        ).send();

        vote(user, author, permlink);
    }



    /// @abi action
    // Voting for a post with a waste of vesting from media-account
    // User vesting balances saved with help of deposit account
    void vote3(account_name user, account_name author, permlink_name permlink) {
        auto value = asset(1, symbol_type(S(4,SYS)));
        action(
            permission_level{N(geos.deposit), N(trx)}, 
            N(geos.deposit), N(transfer), 
            std::make_tuple(
                user, N(eosio.null),
                value,
                std::string("Payment for voting ")+name{author}.to_string()+"@"+permlink.to_string())
        ).send();

        vote(user, author, permlink);
    }



    /// @abi action
    // Voting for a post with a waste of vesting from media-account
    // User vesting balances saved in media account
    void vote4(account_name user, account_name author, permlink_name permlink) {
        auto value = asset(1, symbol_type(S(4,SYS)));

        balances table(_self, user);
        auto index = table.get_index<N(bysymbol)>();
        auto item = index.find(value.symbol.name());
        eosio_assert(item != index.end(), "No balance for user");
        eosio_assert(item->quantity.amount >= value.amount, "No fund on balance for user");
        index.modify(item, _self, [&](auto& b) {
            b.quantity.amount -= value.amount;
        });

        vote(user, author, permlink);
    }



    /// @abi action
    // Pay rewards for post
    void reward(account_name user) {
        print("Reward for user ", name{user}, "\n");
        postrewards rewards(_self, user);
        auto index = rewards.get_index<N(byreward)>();

        auto curr_time = now();

        // Calculate and transfer rewards
        for(auto iter = index.begin(); iter != index.end() && iter->reward_time <= curr_time; ) {
            auto j = iter; iter++;
            print("    ", name{j->permlink}, ": ", j->votes, "\n");
            reward_author(user, j->permlink, j->votes);
            index.erase(j);
        }

        // Rechedule deferred transaction if exists post rewards for this author
        if(index.begin() != index.end()) {
            transaction out{};
            out.actions.emplace_back(
                    permission_level{user, N(active)}, 
                    _self, N(reward), 
                    std::make_tuple(user));
            out.delay_sec = index.begin()->reward_time - curr_time;
            out.send(user, _self);
        }
    }

    void reward_author(account_name author, permlink_name permlink, uint64_t votes) {
        auto data = get_data();
        symbol_name sym = symbol_type(S(4,SYS)).name();
        asset reward_balance = eosio::token(N(eosio.token)).get_balance(N(geos.reward), sym);
        asset quantity = asset(0, sym);
        eosio_assert(data->total_votes >= votes, "Inconsistent votes");
        if(data->total_votes != 0)
            quantity = reward_balance * (int64_t)votes / data->total_votes;
        print("  post reward balance: ", reward_balance);
        //print("  post reward:         ", quantity);
        modify_data([&](auto& o) {
            o.total_votes -= votes;
        });
        
        if(quantity.amount != 0) {
            action(
                permission_level{N(geos.reward), N(active)}, 
                N(eosio.token), N(transfer), 
                std::make_tuple(
                    N(geos.reward), author,
                    quantity,
                    std::string("Reward for post ")+name{author}.to_string()+"@"+permlink.to_string())
            ).send();
        }
    }



    // Struct and table for calculate user funds on media-account
    /// @abi table
    struct balance {
        uint64_t     id;
        asset        quantity;

        uint64_t primary_key() const {return id;}
        uint64_t by_symbol() const {return quantity.symbol.name();}
        EOSLIB_SERIALIZE(balance, (id)(quantity))
    };
    typedef eosio::multi_index<N(balance), balance,
        indexed_by<N(bysymbol), const_mem_fun<balance, uint64_t, &balance::by_symbol> > > balances;



    // Struct and table for calculate post rewards
    /// @abi table
    struct postreward {
        permlink_name permlink;
        uint32_t      reward_time;
        uint32_t      votes;

        uint64_t primary_key() const {return permlink;}
        uint128_t by_reward_time() const {return (((uint128_t)reward_time) << 64) | permlink;}

        EOSLIB_SERIALIZE(postreward, (permlink)(reward_time)(votes))
    };
    typedef eosio::multi_index<N(postreward), postreward,
        indexed_by<N(byreward), const_mem_fun<postreward, uint128_t, &postreward::by_reward_time> > > postrewards;



    // @abi table
    struct data {
        uint64_t id;
        uint32_t total_votes;

        uint64_t primary_key() const {return id;}
        EOSLIB_SERIALIZE(data, (id)(total_votes))
    };
    typedef eosio::multi_index<N(data), data> data_table;
    
    void create_data() {
        data_table table(_self, _self);
        auto iter = table.find(1);
        if (iter == table.end()) {
            table.emplace(_self, [&](auto& o) {
                o.id = 1;
                o.total_votes = 0;
            });
        }
    }

    data_table::const_iterator get_data() {
        data_table table(_self, _self);
        auto iter = table.find(1);
        if(iter == table.end()) {
            create_data();
            iter = table.find(1);
        }
        return iter;
    }

    template<typename Lambda>
    void modify_data(Lambda&& updater) {
        data_table table(_self, _self);
        auto iter = table.find(1);
        if (iter == table.end()) {
            create_data();
            iter = table.find(1);
        }
        table.modify(iter, _self, updater);
    }

};


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

EOSIO_ABI_EX( media, (publish)(vote)(vote2)(vote3)(vote4)(onerror)(reward) )
