#include <eosiolib/eosio.hpp>
#include "../math_expr_test/types.h"
#include "exttypes.h"
#include <string>
#include <vector>
#include <numeric> 
#include <limits> 
   
#include "../math_expr_test/expr_code_info.h"
using namespace exprinfo;   
    
constexpr int ONE_HUNDRED_PERCENT = 10000;
constexpr fixp_t MIN_VESTING_FOR_VOTE = fixp_t(3000);

//@abi table 
struct balance { 
    account_name user;
    int64_t tokenamount = 0; 
    int64_t vestamount = 0;
    
    balance& operator+=(const balance& rhs) {
        eosio_assert(user == rhs.user, "balance, operator+= : wrong user"); 
        tokenamount += rhs.tokenamount;
        vestamount += rhs.vestamount;
        return *this;
    }
    
    uint64_t primary_key() const { return user; }
    EOSLIB_SERIALIZE(balance, (user)(tokenamount)(vestamount))
};

struct messagestate {
    base_t absshares = 0;
    base_t netshares = 0;
    base_t voteshares = 0;    
    base_t sumcuratorsw = 0;
    //TODO: base_t reward_weight; -- post bandwidth stuff...
    EOSLIB_SERIALIZE(messagestate, (absshares)(netshares)(voteshares)(sumcuratorsw))
};

//@abi table
struct message {
    uint64_t id;
    uint64_t created; //when    
    messagestate state; 
    uint64_t primary_key() const { return id; }
    EOSLIB_SERIALIZE(message, (id)(created)(state))
};

//@abi table
struct vote { 
    uint64_t id;
    uint64_t msgid;
    account_name voter;
    base_t weight;
    uint64_t primary_key() const { return id; }
    uint64_t by_message() const { return msgid; }
    EOSLIB_SERIALIZE(vote, (id)(msgid)(voter)(weight))
};

struct rewardrules {
    bytecode mainfunc;
    bytecode curationfunc;
    bytecode timepenalty;    
    base_t curatorsprop;
    EOSLIB_SERIALIZE(rewardrules, (mainfunc)(curationfunc)(timepenalty)(curatorsprop)) 
    };
 
using counter_t = uint64_t;
  
struct poolstate {
    counter_t msgs = 0;
    int64_t funds = 0;    
    base_t rshares = 0;
    base_t rsharesfn = 0;
    
    fixp_t get_ratio() const {
        eosio_assert(funds >= fixp_t(0), "poolstate::get_ratio: funds < 0");
        auto r = FP(rshares);
        return (r > 0) ? static_cast<fixp_t>(fixp_t(funds) / r) : std::numeric_limits<fixp_t>::max();
    }
    EOSLIB_SERIALIZE(poolstate, (msgs)(funds)(rshares)(rsharesfn))
};   
 //@abi table
 struct rewardpool {
    uint64_t created;
    rewardrules rules;
    poolstate state;
    
    uint64_t primary_key() const { return created; }
    EOSLIB_SERIALIZE(rewardpool, (created)(rules)(state))
};
 
using balances = eosio::multi_index<N(balances), balance>;
using messages = eosio::multi_index<N(messages), message>;
using votes = eosio::multi_index<N(votes), vote, 
    eosio::indexed_by<N(bymessage), eosio::const_mem_fun<vote, uint64_t, &vote::by_message> > >;
using reward_pools = eosio::multi_index<N(rewardpools), rewardpool>;

class forum : public eosio::contract {
    
public://temporarily  
    /// @abi action
    void payto(account_name user, int64_t amount, enum_t mode) {
        require_auth(_self);
        balances bs(_self, _self);
        balance add {user};
        switch (static_cast<payment_t>(mode)) {
                case payment_t::TOKEN:   add.tokenamount = amount; break;           
                case payment_t::VESTING: add.vestamount = amount;  break;
                default: eosio_assert(false, "forum::payto: unknown kind of payment");
        }
        auto b = bs.find(user);
        if(b != bs.end())
            bs.modify(b, _self, [&](auto &item) { item += add; }); 
        else
            bs.emplace(_self, [&](auto &item) { item = add; });
    }
private:    
    int64_t get_vesting(account_name user)const {
        balances bs(_self, _self);
        auto b = bs.find(user);
        return (b != bs.end()) ? b->vestamount : 0;
    } 
    
    int64_t pay_curators(account_name author, uint64_t msgid, int64_t max_rewards, fixp_t weights_sum) {
        votes vs(_self, author); 
        int64_t unclaimed_rewards = max_rewards;
                
        auto idx = vs.get_index<N(bymessage)>();
        auto v = idx.lower_bound(msgid);
        while ((v != idx.end()) && (v->msgid == msgid)) {
            if((weights_sum > fixp_t(0)) && (max_rewards > 0)) {
                
                //TODO: implement wide base types for such calculations (https://github.com/GolosChain/golos-smart/issues/71)
                auto claim = static_cast<int64_t>(static_cast<fixp_t>(max_rewards) * static_cast<fixp_t>(FP(v->weight) / weights_sum));
                eosio_assert(claim <= unclaimed_rewards, 
                ("LOGIC ERROR! forum::pay_curators: claim(" + 
                std::to_string(claim) + ") > " + "unclaimed_rewards(" + 
                std::to_string(unclaimed_rewards) + ")").c_str()); 
                if(claim > 0) {
                    unclaimed_rewards -= claim;
                    payto(v->voter, claim, static_cast<enum_t>(payment_t::TOKEN));
                }
            }
            v = idx.erase(v);
        }
        return unclaimed_rewards;
    }
    
    auto get_pool(reward_pools& pools, uint64_t time) {        
        eosio_assert(pools.begin() != pools.end(), "forum::get_pool: [pools] is empty");
        
        auto pool = pools.upper_bound(time);
        
        eosio_assert(pool != pools.begin(), "forum::get_pool: can't find an appropriate pool");        
        return (--pool);
    }
    

public:
    using contract::contract;
           
     /// @abi action
    void setrules(const std::string& mainfunc, const std::string& curationfunc, const std::string& timepenalty, int64_t curatorsprop)
        //TODO: machine's constants
    {   
        require_auth(_self);
        reward_pools pools(_self, _self); 
        uint64_t created = current_time();
        
        auto old_pool = pools.begin();
        int64_t unclaimed_funds = 0;
        while(old_pool != pools.end())
            if(!old_pool->state.msgs) {
                unclaimed_funds += old_pool->state.funds;
                old_pool = pools.erase(old_pool);                
            }
            else
                ++old_pool;       

        eosio_assert(pools.find(created) == pools.end(), "rules with this key already exist");
        std::string vars_str("x");
        rewardrules newrules;
        atmsp::parser<fixp_t> pa;
        atmsp::machine<fixp_t> bc;
        
        pa(bc, mainfunc, vars_str);        
        newrules.mainfunc.from_machine(bc);
        
        pa(bc, curationfunc, vars_str);        
        newrules.curationfunc.from_machine(bc);
        
        pa(bc, timepenalty, vars_str);        
        newrules.timepenalty.from_machine(bc);   
        
        newrules.curatorsprop = (static_cast<fixp_t>(static_cast<fixp_t>(curatorsprop) / static_cast<fixp_t>(ONE_HUNDRED_PERCENT))).data();
              
        pools.emplace(_self, [&](auto &item) { item = {.created = created, .rules = newrules, .state = {.msgs = 0, .funds = unclaimed_funds} }; });
}
    
    /// @abi action
    void issue(int64_t amount) {        
        require_auth(_self); //TODO: issuer smart contract account        
        
        reward_pools pools(_self, _self);
        auto pool = pools.begin();
        eosio_assert(pool != pools.end(), "forum::issue: [pools] is empty");
        auto choice = pool;
        auto min_ratio = choice->state.get_ratio();
        while((++pool) != pools.end()) {
            auto cur_ratio = pool->state.get_ratio();
            if(cur_ratio <= min_ratio) {
                min_ratio = cur_ratio;
                choice = pool;
            }
        }        
        pools.modify(*choice, _self, [&](auto &item){ item.state.funds += amount; });
    }
    
    /// @abi action
    void addmessage(account_name author, uint64_t id) {
        require_auth(author);
           
        reward_pools pools(_self, _self);
        auto pool = pools.rbegin();
        eosio_assert(pool != pools.rend(), "forum::addmessage: [pools] is empty");
        auto cur_time = current_time();
        eosio_assert(cur_time >= pool->created, "forum::addmessage: cur_time < pool.created");
         
        constexpr auto max_counter_val = std::numeric_limits<counter_t>::max();
        if(pool->state.msgs == max_counter_val)
            eosio_assert(pool != pools.rend(), "forum::addmessage: pool->msgs == max_counter_val");
       
        messages msgs(_self, author);        
        eosio_assert(msgs.find(id) == msgs.end(), ("forum::addmessage: id already exist:" + std::to_string(id)).c_str());
                
        pools.modify(*pool, _self, [&](auto &item){ item.state.msgs++; });        
        msgs.emplace(_self, [&](auto &item) { item = {.id = id, .created = cur_time}; });
    }
   
    static fixp_t get_delta(fixp_t old_val, fixp_t new_val, const bytecode& bc) {

        atmsp::machine<fixp_t> mchn;
        bc.to_machine(mchn);
        
        mchn.var[0] = old_val; 
        fixp_t old_fn = mchn.run();        
        mchn.var[0] = new_val;
        fixp_t new_fn = mchn.run();
        
        return (new_fn - old_fn);
   }
      
    /// @abi action
    void addvote(account_name author, uint64_t msgid, account_name voter, int64_t weight) {
        //TODO: revote
        require_auth(voter);
        votes vs(_self, author);
        
        messages msgs(_self, author); 
        auto msg = msgs.find(msgid);    
        eosio_assert(msg != msgs.end(), "forum::addvote: can't find message");
        
        fixp_t abs_w = static_cast<fixp_t>(abs(weight))  / static_cast<fixp_t>(ONE_HUNDRED_PERCENT);
        
        fixp_t current_power = fixp_t(1); // TODO
        fixp_t used_power = current_power * abs_w; //TODO: max_vote_denom and so on
        fixp_t abs_rshares = fixp_t(get_vesting(voter)) * used_power; 
       
        //TODO: if weight == 0 (revote case) we should pass it
        eosio_assert(abs_rshares >= MIN_VESTING_FOR_VOTE, "forum::addvote: abs_rshares < MIN_VESTING_FOR_VOTE");
              
        fixp_t rshares = (weight < 0) ? -abs_rshares : abs_rshares;
        
        messagestate msg_new_state = {
            .absshares = (FP(msg->state.absshares) + abs_rshares).data(),
            .netshares = (FP(msg->state.netshares) + rshares).data(),
            .voteshares = ((rshares > fixp_t(0)) ? FP(msg->state.voteshares) + rshares : FP(msg->state.voteshares)).data()
            //.sumcuratorsw = see below
        };
        reward_pools pools(_self, _self);
        auto pool = get_pool(pools, msg->created);
        
        auto rsharesfn_delta = get_delta(FP(msg->state.netshares), FP(msg_new_state.netshares), pool->rules.mainfunc);
                
        pools.modify(*pool, _self, [&](auto &item) {
             item.state.rshares = (FP(item.state.rshares) + rshares).data();
             item.state.rsharesfn =  (FP(item.state.rsharesfn) + rsharesfn_delta).data();
        });
        
        auto sumcuratorsw_delta = get_delta(FP(msg->state.voteshares), FP(msg_new_state.voteshares), pool->rules.curationfunc);
                    
        msg_new_state.sumcuratorsw = (FP(msg->state.sumcuratorsw) + sumcuratorsw_delta).data();
        
        msgs.modify(msg, _self, [&](auto &item) { item.state = msg_new_state; });     
        
        //TODO: check if this vote already exist 
        
        vs.emplace(_self, [&](auto &item) {
            item = {
                .id = vs.available_primary_key(),    
                .msgid = msgid,
                .voter = voter,
                .weight = sumcuratorsw_delta.data()
            }; 
        });
    }
            
    /// @abi action
    void payrewards(account_name author, uint64_t id) {
        require_auth(_self);
        
        messages msgs(_self, author);
        auto msg = msgs.find(id);   
        
        eosio_assert(msg != msgs.end(), "forum::payrewards: can't find message");
         
        reward_pools pools(_self, _self);
        auto pool = get_pool(pools, msg->created);       
        eosio_assert(pool->state.msgs != 0, "LOGIC ERROR! forum::payrewards: pool.msgs is equal to zero");
        
        atmsp::machine<fixp_t> reward_func;
        pool->rules.mainfunc.to_machine(reward_func);
        reward_func.var[0] = FP(msg->state.netshares);
        fixp_t sharesfn = reward_func.run();
        
        auto state = pool->state;
        
        int64_t payout = 0;
        if(state.msgs == 1) {
            payout = state.funds;
            eosio_assert(state.rshares == msg->state.netshares, "LOGIC ERROR! forum::payrewards: pool->rshares != msg->netshares for last message"); 
            eosio_assert(state.rsharesfn == sharesfn.data(), "LOGIC ERROR! forum::payrewards: pool->rsharesfn != sharesfn.data() for last message"); 
            state.funds = 0;
            state.rshares = 0; 
            state.rsharesfn = 0;
        } 
        else if(sharesfn > fixp_t(0)) { 
            auto total_rsharesfn = FP(state.rsharesfn);
            eosio_assert(total_rsharesfn > fixp_t(0), "LOGIC ERROR! forum::payrewards: total_rshares_fn <= 0"); 
           
            //TODO: implement wide base types (https://github.com/GolosChain/golos-smart/issues/71)
            payout = static_cast<int64_t>(static_cast<fixp_t>(state.funds) * static_cast<fixp_t>(sharesfn / total_rsharesfn));
                          
            state.funds -= payout;
            
            eosio_assert(state.funds >= 0, "LOGIC ERROR! forum::payrewards: state.funds < 0"); 
            
            auto new_rshares = FP(state.rshares) - FP(msg->state.netshares);
            auto new_rsharesfn = FP(state.rsharesfn) - sharesfn;
            
            eosio_assert(new_rshares >= fixp_t(0), "LOGIC ERROR! forum::payrewards: new_rshares < 0"); 
            eosio_assert(new_rsharesfn >= fixp_t(0), "LOGIC ERROR! forum::payrewards: new_rsharesfn < 0"); 
            
            state.rshares = new_rshares.data();
            state.rsharesfn = new_rsharesfn.data();            
        }
        
        auto curation_payout = static_cast<int64_t>(FP(pool->rules.curatorsprop) * static_cast<fixp_t>(payout));
        eosio_assert((curation_payout <= payout) && (curation_payout >= 0), ("forum::payrewards: wrong curation_payout = " 
            + std::to_string(curation_payout) + " (payout = " + std::to_string(payout) + ")").c_str());
         
        auto unclaimed_rewards = pay_curators(author, id, curation_payout, FP(msg->state.sumcuratorsw));
           
        eosio_assert(unclaimed_rewards >= 0, "forum::payrewards: unclaimed_rewards < 0");
           
        state.funds += unclaimed_rewards;
        
        payout -= curation_payout;
        payto(author, payout, static_cast<enum_t>(payment_t::TOKEN));
        
        msgs.erase(msg);   
        
        bool pool_erased = false;
        state.msgs--;      
        if(state.msgs == 0) {
            auto next = pool;
            if((++next) != pools.end()) {                
                eosio_assert(state.funds == unclaimed_rewards, "LOGIC ERROR! forum::payrewards: state.funds != unclaimed_rewards"); 
                
                if(state.funds > 0)
                    pools.modify(next, _self, [&](auto &item) { item.state.funds += state.funds; });    
                      
                pools.erase(pool);
                pool_erased = true;          
            }
        } 
        
        if(!pool_erased)
            pools.modify(pool, _self, [&](auto &item) { item.state = state; });      
    }
};
EOSIO_ABI(forum, (payto)(setrules)(issue)(addmessage)(addvote)(payrewards)) 
 



