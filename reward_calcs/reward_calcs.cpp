#include <eosiolib/eosio.hpp>
#include <eosiolib/transaction.hpp>
#include <string>
#include <vector>
#include <numeric> 
#include <limits>

#include "types.h"
#include "config.h"

#include <common/calclib/atmsp_storable.h>
#include <eosio.token/eosio.token.hpp>
#include <golos.vesting/golos.vesting.hpp>

using namespace atmsp::storable;
using namespace golos::config;
using namespace fixed_point_utils;
using counter_t = uint64_t;

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

struct [[eosio::table]] messagestate {
    base_t absshares = 0;
    base_t netshares = 0;
    base_t voteshares = 0;    
    base_t sumcuratorsw = 0;  
};

struct [[eosio::table]] message {
    uint64_t id;
    uint64_t created; //when
    base_t tokenprop; //elaf_t
    messagestate state;
    uint64_t primary_key() const { return id; }
};

struct [[eosio::table]] vote { 
    uint64_t id;
    uint64_t msgid;
    account_name voter;
    base_t weight;
    uint64_t primary_key() const { return id; }
    uint64_t by_message() const { return msgid; }
};
  
struct [[eosio::table]] funcinfo {
    bytecode code;
    base_t maxarg;
};

struct [[eosio::table]] rewardrules {
    funcinfo mainfunc;
    funcinfo curationfunc; 
    funcinfo timepenalty;
    base_t curatorsprop; //elaf_t
    base_t maxtokenprop; //elaf_t
};
  
struct [[eosio::table]] poolstate {
    counter_t msgs;
    eosio::asset funds;    
    base_t rshares;
    base_t rsharesfn;    
    
    using ratio_t = decltype(fixp_t(funds.amount) / FP(rshares));
    ratio_t get_ratio() const {
        eosio_assert(funds.amount >= 0, "poolstate::get_ratio: funds < 0");
        auto r = FP(rshares);     
        return (r > 0) ? (fixp_t(funds.amount) / r) : std::numeric_limits<ratio_t>::max();
    }    
};   

 struct [[eosio::table]] rewardpool {
    uint64_t created;
    rewardrules rules;
    poolstate state;
    
    uint64_t primary_key() const { return created; }
    EOSLIB_SERIALIZE(rewardpool, (created)(rules)(state))
};
 
using messages = eosio::multi_index<N(messages), message>;
using votes = eosio::multi_index<N(votes), vote, 
    eosio::indexed_by<N(bymessage), eosio::const_mem_fun<vote, uint64_t, &vote::by_message> > >;
using reward_pools = eosio::multi_index<N(rewardpools), rewardpool>;

class forum : public eosio::contract {
    
    void payto(account_name user, eosio::asset quantity, enum_t mode) {
        require_auth(_self);
        eosio_assert(quantity.amount >= 0, "LOGIC ERROR! forum::payto: quantity.amount < 0");
        if(quantity.amount == 0)
            return;

        if(static_cast<payment_t>(mode) == payment_t::TOKEN)
            INLINE_ACTION_SENDER(eosio::token, transfer) (N(eosio.token), {_self, N(active)}, {_self, user, quantity, ""});
        else if(static_cast<payment_t>(mode) == payment_t::VESTING)
            INLINE_ACTION_SENDER(eosio::token, transfer) (N(eosio.token), {_self, N(active)}, {_self, vesting_name, quantity, name{user}.to_string()});
        else
            eosio_assert(false, "forum::payto: unknown kind of payment");
    } 
     
    int64_t pay_curators(account_name author, uint64_t msgid, int64_t max_rewards, fixp_t weights_sum, eosio::symbol_type tokensymbol) {
        votes vs(_self, author);
        int64_t unclaimed_rewards = max_rewards;
                
        auto idx = vs.get_index<N(bymessage)>();
        auto v = idx.lower_bound(msgid);
        while ((v != idx.end()) && (v->msgid == msgid)) {
            if((weights_sum > fixp_t(0)) && (max_rewards > 0)) {
                auto claim = int_cast(elai_t(max_rewards) * elaf_t(FP(v->weight) / weights_sum));
                
                //eosio_assert(claim <= unclaimed_rewards, ("LOGIC ERROR! forum::pay_curators: claim(" + 
                 //   std::to_string(claim) + ") > " + "unclaimed_rewards(" + std::to_string(unclaimed_rewards) + ")").c_str()); 
                //^to_string isn't working for now: https://github.com/EOSIO/eosio.cdt/issues/95
                eosio_assert(claim <= unclaimed_rewards, "LOGIC ERROR! forum::pay_curators: claim > unclaimed_rewards");
                
                if(claim > 0) {
                    unclaimed_rewards -= claim;
                    payto(v->voter, eosio::asset(claim, tokensymbol), static_cast<enum_t>(payment_t::VESTING));
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
    
    void fill_depleted_pool(reward_pools& pools, eosio::asset quantity, reward_pools::const_iterator excluded) {
        eosio_assert(quantity.amount >= 0, "forum::fill_depleted_pool: quantity.amount < 0");
        if(quantity.amount == 0)
            return;
        auto choice = pools.end();
        auto min_ratio = std::numeric_limits<poolstate::ratio_t>::max();
        for(auto pool = pools.begin(); pool != pools.end(); ++pool)
            if((pool->state.funds.symbol == quantity.symbol) && (pool != excluded)) {
                auto cur_ratio = pool->state.get_ratio();
                if(cur_ratio <= min_ratio) {
                    min_ratio = cur_ratio;
                    choice = pool;
                }
            }
        //sic. we don't need assert here
        if(choice != pools.end())
            pools.modify(*choice, _self, [&](auto &item){ item.state.funds += quantity; });
    }
    
    static fixp_t calc_1d(atmsp::machine<fixp_t>& machine, fixp_t arg) {
        machine.var[0] = std::max(fixp_t(0), arg);
        return machine.run();
    }
    
    static fixp_t get_delta(fixp_t old_val, fixp_t new_val, const funcinfo& func) {
        atmsp::machine<fixp_t> mchn;
        func.code.to_machine(mchn);        
        elap_t old_fn = calc_1d(mchn, std::min(old_val, FP(func.maxarg)));   
        elap_t new_fn = calc_1d(mchn, std::min(new_val, FP(func.maxarg)));
        return fp_cast<fixp_t>(new_fn - old_fn, false);
    }
    
    static fixp_t add_cut(fixp_t lhs, fixp_t rhs) {
        return fp_cast<fixp_t>(elap_t(lhs) + elap_t(rhs), false);
    }
    
    static elaf_t get_limit_prop(int64_t arg) {
        eosio_assert(arg >= 0, "forum::get_limit_prop: arg < 0");
        return std::min(elaf_t(elai_t(arg) / elai_t(ONE_HUNDRED_PERCENT)), elaf_t(1));
    }
   
    static void check_positive_monotonic(atmsp::machine<fixp_t>& machine, fixp_t max_arg, const std::string& name, bool inc) {
        fixp_t prev_res = calc_1d(machine, max_arg);
        if(!inc)
            eosio_assert(prev_res >= fixp_t(0), ("forum::check positive failed for " + name).c_str());
        fixp_t cur_arg = max_arg;
        for(size_t i = 0; i < CHECK_MONOTONIC_STEPS; i++) {
            cur_arg /= fixp_t(2);           
            fixp_t cur_res = calc_1d(machine, cur_arg);
            
            eosio_assert(inc ? (cur_res <= prev_res) : (cur_res >= prev_res), ("forum::check monotonic failed for " + name).c_str());
            prev_res = cur_res;
        }
        fixp_t res_zero = calc_1d(machine, fixp_t(0));
        if(inc)
            eosio_assert(res_zero >= fixp_t(0), ("forum::check positive failed for " + name).c_str());
        eosio_assert(inc ? (res_zero <= prev_res) : (res_zero >= prev_res), ("forum::check monotonic [0] failed for " + name).c_str());
    }
    
    static funcinfo load_func(const funcparams& params, const std::string& name, const atmsp::parser<fixp_t>& pa, atmsp::machine<fixp_t>& bc, bool inc) {
        eosio_assert(params.maxarg > 0, "forum::load_func: params.maxarg <= 0");
        funcinfo ret;     
        ret.maxarg = fp_cast<fixp_t>(params.maxarg).data();
        pa(bc, params.str, "x");
        check_positive_monotonic(bc, FP(ret.maxarg), name, inc);
        ret.code.from_machine(bc);
        return ret;
    }
    
public:
    using contract::contract; 
           
    [[eosio::action]]
    void setrules(const funcparams& mainfunc, const funcparams& curationfunc, const funcparams& timepenalty, 
    int64_t curatorsprop, int64_t maxtokenprop,
    eosio::symbol_type tokensymbol) {
        //TODO: machine's constants
        require_auth(_self);        
        reward_pools pools(_self, _self);   
        uint64_t created = current_time();
        
        eosio::asset unclaimed_funds = eosio::token(N(eosio.token)).get_balance(_self, tokensymbol.name());

        auto old_pool = pools.begin();
        while(old_pool != pools.end())
            if(!old_pool->state.msgs)                
                old_pool = pools.erase(old_pool);            
            else {
                if(old_pool->state.funds.symbol == tokensymbol)
                    unclaimed_funds -= old_pool->state.funds;
                ++old_pool;
            }                   

        eosio_assert(pools.find(created) == pools.end(), "rules with this key already exist");
        
        rewardrules newrules;
        atmsp::parser<fixp_t> pa;
        atmsp::machine<fixp_t> bc;
        
        newrules.mainfunc     = load_func(mainfunc, "reward func", pa, bc, true);
        newrules.curationfunc = load_func(curationfunc, "curation func", pa, bc, true);
        newrules.timepenalty  = load_func(timepenalty, "time penalty func", pa, bc, false);
        
        newrules.curatorsprop = static_cast<base_t>(get_limit_prop(curatorsprop).data());
        newrules.maxtokenprop = static_cast<base_t>(get_limit_prop(maxtokenprop).data());   
              
        pools.emplace(_self, [&](auto &item) {
            item.created = created; 
            item.rules = newrules; 
            item.state.msgs = 0; 
            item.state.funds = unclaimed_funds;
            item.state.rshares = (fixp_t(0)).data();
            item.state.rsharesfn = (fixp_t(0)).data(); 
        });
    }

    void on_transfer(account_name from, account_name to, eosio::asset quantity, std::string memo) {        
        (void)memo;        
        require_auth(from);  
        if(_self != to)
            return;  

        reward_pools pools(_self, _self); 
        fill_depleted_pool(pools, quantity, pools.end()); 
    }
    
    [[eosio::action]]
    void addmessage(account_name author, uint64_t id, int64_t tokenprop) {
        require_auth(author);
           
        reward_pools pools(_self, _self);
        auto pool = pools.rbegin();
        eosio_assert(pool != pools.rend(), "forum::addmessage: [pools] is empty");
        auto cur_time = current_time();
        eosio_assert(cur_time >= pool->created, "forum::addmessage: cur_time < pool.created");
        eosio_assert(pool->state.msgs < std::numeric_limits<counter_t>::max(), "forum::addmessage: pool->msgs == max_counter_val");
       
        messages msgs(_self, author);
        
        //eosio_assert(msgs.find(id) == msgs.end(), ("forum::addmessage: id already exist:" + std::to_string(id)).c_str());         
        //to_string isn't working for now: https://github.com/EOSIO/eosio.cdt/issues/95
        eosio_assert(msgs.find(id) == msgs.end(), "forum::addmessage: id already exist");        
        
        pools.modify(*pool, _self, [&](auto &item){ item.state.msgs++; });
        msgs.emplace(_self, [&](auto &item) { item = {
            .id = id, 
            .created = cur_time, 
            .tokenprop = static_cast<base_t>(std::min(get_limit_prop(tokenprop), ELF(pool->rules.maxtokenprop)).data())
            }; 
        });
    }
      
    [[eosio::action]]
    void addvote(account_name author, uint64_t msgid, account_name voter, int64_t weight) {
        //TODO: revote
        require_auth(voter);
        votes vs(_self, author);
        
        messages msgs(_self, author); 
        auto msg = msgs.find(msgid);    
        eosio_assert(msg != msgs.end(), "forum::addvote: can't find message");
        
        auto abs_w = get_limit_prop(abs(weight));
        
        reward_pools pools(_self, _self);
        auto pool = get_pool(pools, msg->created);
        
        elaf_t current_power = 1; // TODO
        elaf_t used_power = current_power * abs_w; //TODO: max_vote_denom and so on
        fixp_t abs_rshares = fp_cast<fixp_t>(eosio::vesting(vesting_name).get_account_vesting(voter, pool->state.funds.symbol).amount, false) * used_power;
       
        //TODO: if weight == 0 (revote case) we should pass it
        eosio_assert(abs_rshares >= MIN_VESTING_FOR_VOTE, "forum::addvote: abs_rshares < MIN_VESTING_FOR_VOTE");
              
        fixp_t rshares = (weight < 0) ? -abs_rshares : abs_rshares;
        
        messagestate msg_new_state = {
            .absshares = add_cut(FP(msg->state.absshares), abs_rshares).data(),
            .netshares = add_cut(FP(msg->state.netshares), rshares).data(),
            .voteshares = ((rshares > fixp_t(0)) ? 
                add_cut(FP(msg->state.voteshares), rshares) :
                FP(msg->state.voteshares)).data()
            //.sumcuratorsw = see below
        };        
        
        auto rsharesfn_delta = get_delta(FP(msg->state.netshares), FP(msg_new_state.netshares), pool->rules.mainfunc);
                
        pools.modify(*pool, _self, [&](auto &item) {
             item.state.rshares = add_cut(FP(item.state.rshares), rshares).data();//(FP(item.state.rshares) + rshares).data();
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
            
    [[eosio::action]]
    void payrewards(account_name author, uint64_t id) {
        require_auth(_self);
        
        messages msgs(_self, author);
        auto msg = msgs.find(id);   
        
        eosio_assert(msg != msgs.end(), "forum::payrewards: can't find message");
         
        reward_pools pools(_self, _self);
        auto pool = get_pool(pools, msg->created);       
        eosio_assert(pool->state.msgs != 0, "LOGIC ERROR! forum::payrewards: pool.msgs is equal to zero");
        
        atmsp::machine<fixp_t> reward_func;
        pool->rules.mainfunc.code.to_machine(reward_func);
        fixp_t sharesfn = calc_1d(reward_func, std::min(FP(msg->state.netshares), FP(pool->rules.mainfunc.maxarg)));
        
        auto state = pool->state;
        
        int64_t payout = 0;
        if(state.msgs == 1) {
            payout = state.funds.amount;
            eosio_assert(state.rshares == msg->state.netshares, "LOGIC ERROR! forum::payrewards: pool->rshares != msg->netshares for last message"); 
            eosio_assert(state.rsharesfn == sharesfn.data(), "LOGIC ERROR! forum::payrewards: pool->rsharesfn != sharesfn.data() for last message"); 
            state.funds.amount = 0;
            state.rshares = 0; 
            state.rsharesfn = 0;
        } 
        else if(sharesfn > fixp_t(0)) { 
            auto total_rsharesfn = FP(state.rsharesfn);
            eosio_assert(total_rsharesfn > fixp_t(0), "LOGIC ERROR! forum::payrewards: total_rshares_fn <= 0"); 
           
            payout = int_cast(elai_t(state.funds.amount) * static_cast<elaf_t>(sharesfn / total_rsharesfn));
                          
            state.funds.amount -= payout;
            
            eosio_assert(state.funds.amount >= 0, "LOGIC ERROR! forum::payrewards: state.funds < 0"); 
            
            auto new_rshares = FP(state.rshares) - FP(msg->state.netshares);
            auto new_rsharesfn = FP(state.rsharesfn) - sharesfn;
            
            eosio_assert(new_rshares >= fixp_t(0), "LOGIC ERROR! forum::payrewards: new_rshares < 0"); 
            eosio_assert(new_rsharesfn >= fixp_t(0), "LOGIC ERROR! forum::payrewards: new_rsharesfn < 0"); 
            
            state.rshares = new_rshares.data();
            state.rsharesfn = new_rsharesfn.data();            
        }
        auto curation_payout = int_cast(ELF(pool->rules.curatorsprop) * elai_t(payout));
        
        //eosio_assert((curation_payout <= payout) && (curation_payout >= 0), ("forum::payrewards: wrong curation_payout = " 
        //    + std::to_string(curation_payout) + " (payout = " + std::to_string(payout) + ")").c_str());        
        eosio_assert((curation_payout <= payout) && (curation_payout >= 0), "forum::payrewards: wrong curation_payout");
         
        auto unclaimed_rewards = pay_curators(author, id, curation_payout, FP(msg->state.sumcuratorsw), state.funds.symbol);
           
        eosio_assert(unclaimed_rewards >= 0, "forum::payrewards: unclaimed_rewards < 0");
           
        state.funds.amount += unclaimed_rewards;
        
        payout -= curation_payout;
        auto token_payout = int_cast(elai_t(payout) * ELF(msg->tokenprop));
        eosio_assert(payout >= token_payout, "forum::payrewards: wrong token_payout value");
        
        payto(author, eosio::asset(token_payout, state.funds.symbol), static_cast<enum_t>(payment_t::TOKEN));
        payto(author, eosio::asset(payout - token_payout, state.funds.symbol), static_cast<enum_t>(payment_t::VESTING));
        
        msgs.erase(msg);   
        
        bool pool_erased = false;
        state.msgs--;      
        if(state.msgs == 0) {
            if(pool != --pools.end()) {//there is a pool after, so we can delete this one        
                eosio_assert(state.funds.amount == unclaimed_rewards, "LOGIC ERROR! forum::payrewards: state.funds != unclaimed_rewards");                
                fill_depleted_pool(pools, state.funds, pool);
                pools.erase(pool);
                pool_erased = true;             
            }
        }        
        if(!pool_erased)
            pools.modify(pool, _self, [&](auto &item) { item.state = state; });      
    }
    
    void onerror() {
        eosio::onerror error = eosio::onerror::from_current_action();
}
};
EOSIO_ABI_EX(forum, (setrules)(addmessage)(addvote)(payrewards)(onerror))




 



