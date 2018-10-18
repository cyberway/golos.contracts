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
template<typename T, typename F>
auto get_itr(T& tbl, uint64_t key, account_name payer, F&& insert_fn) {    
    auto itr = tbl.find(key);
    if(itr == tbl.end())
        itr = tbl.emplace(payer, insert_fn);
    return itr;    
}

struct [[eosio::table]] messagestate {
    base_t absshares = 0;
    base_t netshares = 0;
    base_t voteshares = 0;
    base_t sumcuratorsw = 0;
};

struct [[eosio::table]] beneficiary {
    account_name user;
    base_t prop; //elaf_t
};

struct [[eosio::table]] message {
    uint64_t id;
    uint64_t created; //when
    base_t tokenprop; //elaf_t
    std::vector<beneficiary> beneficiaries;
    base_t rewardweight;//elaf_t
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
    //uint64_t cashout_time; //TODO:
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

struct [[eosio::table]] limits {
    enum kind_t: enum_t {POST, COMM, VOTE, POSTBW, UNDEF};
    std::vector<bytecode> restorers;//(funcs of: prev_charge (p), vesting (v), elapsed_seconds (t))
    std::vector<limitedact> limitedacts;
    std::vector<int64_t> vestingprices;//disabled if < 0
    std::vector<int64_t> minvestings;
    
    const limitedact& get_limited_act(kind_t kind)const {
        auto k = static_cast<size_t>(kind);
        eosio_assert(k < limitedacts.size(), "limits: undefined kind");
        return limitedacts[k];
    }
    
    const bytecode* get_restorer(kind_t kind)const {
        uint8_t num = get_limited_act(kind).restorernum;
        if(num < restorers.size())
            return &(restorers[num]);
        eosio_assert(num == disabled_restorer, "limits: wrong restorer num");
        return nullptr;
    }
    
    int64_t get_vesting_price(kind_t kind)const {
        auto k = static_cast<size_t>(kind);
        return (k < vestingprices.size()) ? vestingprices[k] : -1;
    } 
    
    int64_t get_min_vesting_for(kind_t kind)const {
        auto k = static_cast<size_t>(kind);
        eosio_assert(k < minvestings.size(), "limits: wrong act type");
        return minvestings[k];
    }
    
    size_t get_charges_num()const {
        size_t ret = 0;
        for(auto& act : limitedacts)
            ret = std::max(ret, static_cast<size_t>(act.chargenum) + 1);
        return ret;
    }
    
    void check() {
        eosio_assert(vestingprices.size() < 3, "limits::check: vesting payment may be available only for messages");
        eosio_assert(minvestings.size() == 3, "limits::check: undefined min vesting value");
        for(auto& act : limitedacts)
            eosio_assert((act.restorernum < restorers.size()) || (act.restorernum == disabled_restorer), "limits::check: wrong restorer num");
    }
};

static fixp_t set_and_run(atmsp::machine<fixp_t>& machine, const bytecode& bc, const std::vector<fixp_t>& args, const std::vector<std::pair<fixp_t, fixp_t> >& domain = {}) {
    bc.to_machine(machine);
    return machine.run(args, domain);
}

struct chargeinfo {
    uint64_t lastupdate;
    base_t data;
    
    void update(atmsp::machine<fixp_t>& machine, const bytecode* restorer, uint64_t cur_time, fixp_t vesting, fixp_t price, elaf_t w = elaf_t(1)) {
        if(restorer != nullptr) {
            int64_t elapsed_seconds = static_cast<int64_t>((cur_time - lastupdate) / seconds(1).count());
            auto prev = FP(data);
            
            using namespace limit_restorer_domain;            
            auto restored = set_and_run(machine, *restorer, 
                {prev, vesting, fp_cast<fixp_t>(elapsed_seconds, false)}, 
                {{fixp_t(0), max_prev}, {fixp_t(0), max_vesting}, {fixp_t(0), max_elapsed_seconds}});
            
            if(restored > prev)
                data = fixp_t(0).data();
            else
                data = std::min(fp_cast<fixp_t>(elap_t(FP(data)) - elap_t(restored), false), max_res).data();
        }        
        if(price > fixp_t(0)) {
            auto added = std::max(fp_cast<fixp_t>(elap_t(price) * w, false), std::numeric_limits<fixp_t>::min());
            data = std::min(fp_cast<fixp_t>(elap_t(FP(data)) + elap_t(added), false), limit_restorer_domain::max_res).data();
        }
        lastupdate = cur_time;
    }
};

struct [[eosio::table]] usercharges {
    uint64_t poolcreated; //charges depend on a pool
    std::vector<chargeinfo> vals;

    uint64_t primary_key() const { return poolcreated; }    
    
    elaf_t calc_power(atmsp::machine<fixp_t>& machine, const limits& lims, limits::kind_t kind, uint64_t cur_time, fixp_t vesting, elaf_t w = elaf_t(1)) {
        auto& lim_act = lims.get_limited_act(kind);
        auto restorer = lims.get_restorer(kind);
        eosio_assert(vals.size() > lim_act.chargenum, "usercharges::update: LOGIC ERROR! wrong charge num");
        auto& chg = vals[lim_act.chargenum];
        eosio_assert(chg.lastupdate <= cur_time, "usercharges::update: LOGIC ERROR! chg.lastupdate > cur_time");        
        chg.update(machine, restorer, cur_time, vesting, FP(lim_act.chargeprice), w);
        elap_t cutoff = FP(lim_act.cutoffval);
        elap_t charge = FP(chg.data);
        return (charge > cutoff) ? elaf_t(cutoff / charge) : elaf_t(1);
    }
};

struct [[eosio::table]] rewardpool {
    uint64_t created;
    rewardrules rules;
    limits lims;
    poolstate state;
    
    uint64_t primary_key() const { return created; }
    EOSLIB_SERIALIZE(rewardpool, (created)(rules)(lims)(state)) 
};
 
using messages = eosio::multi_index<N(messages), message>;
using votes = eosio::multi_index<N(votes), vote, 
    eosio::indexed_by<N(bymessage), eosio::const_mem_fun<vote, uint64_t, &vote::by_message> > >;
using reward_pools = eosio::multi_index<N(rewardpools), rewardpool>;
using charges = eosio::multi_index<N(charges), usercharges>;

class forum : public eosio::contract {
    
    void check_account(account_name user, eosio::symbol_type tokensymbol) {
        eosio_assert(eosio::vesting(vesting_name).balance_exist(user, tokensymbol.name()), ("unregistered user: " + name{user}.to_string()).c_str());    
    }
    
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
    
    static fixp_t get_delta(atmsp::machine<fixp_t>& machine, fixp_t old_val, fixp_t new_val, const funcinfo& func) {
        func.code.to_machine(machine);
        elap_t old_fn = machine.run({old_val}, {{fixp_t(0), FP(func.maxarg)}});
        elap_t new_fn = machine.run({new_val}, {{fixp_t(0), FP(func.maxarg)}});
        return fp_cast<fixp_t>(new_fn - old_fn, false);
    }
    
    static fixp_t add_cut(fixp_t lhs, fixp_t rhs) {
        return fp_cast<fixp_t>(elap_t(lhs) + elap_t(rhs), false);
    }
    
    static elaf_t get_limit_prop(int64_t arg) {
        eosio_assert(arg >= 0, "forum::get_limit_prop: arg < 0");
        return elaf_t(elai_t(std::min(arg, ONE_HUNDRED_PERCENT)) / elai_t(ONE_HUNDRED_PERCENT));
    }
    
    static fixp_t get_prop(int64_t arg) {
        return fp_cast<fixp_t>(elai_t(arg) / elai_t(ONE_HUNDRED_PERCENT));
    }
   
    static void check_positive_monotonic(atmsp::machine<fixp_t>& machine, fixp_t max_arg, const std::string& name, bool inc) {
        
        fixp_t prev_res = machine.run({max_arg});
        if(!inc)
            eosio_assert(prev_res >= fixp_t(0), ("forum::check positive failed for " + name).c_str());
        fixp_t cur_arg = max_arg;
        for(size_t i = 0; i < CHECK_MONOTONIC_STEPS; i++) {
            cur_arg /= fixp_t(2);           
            fixp_t cur_res = machine.run({cur_arg});
            eosio_assert(inc ? (cur_res <= prev_res) : (cur_res >= prev_res), ("forum::check monotonic failed for " + name).c_str());
            prev_res = cur_res;
        }
        fixp_t res_zero = machine.run({fixp_t(0)});
        if(inc)
            eosio_assert(res_zero >= fixp_t(0), ("forum::check positive failed for " + name).c_str());
        eosio_assert(inc ? (res_zero <= prev_res) : (res_zero >= prev_res), ("forum::check monotonic [0] failed for " + name).c_str());
    }
    
    static funcinfo load_func(const funcparams& params, const std::string& name, const atmsp::parser<fixp_t>& pa, atmsp::machine<fixp_t>& machine, bool inc) {
        eosio_assert(params.maxarg > 0, "forum::load_func: params.maxarg <= 0");
        funcinfo ret;     
        ret.maxarg = fp_cast<fixp_t>(params.maxarg).data();
        pa(machine, params.str, "x");
        check_positive_monotonic(machine, FP(ret.maxarg), name, inc);
        ret.code.from_machine(machine);
        return ret;
    }
    
    static bytecode load_restorer_func(const std::string str_func, const atmsp::parser<fixp_t>& pa, atmsp::machine<fixp_t>& machine) {
        bytecode ret;
        pa(machine, str_func, "p,v,t");//prev value, vesting, time(elapsed seconds)
        ret.from_machine(machine);
        return ret;
    }
    
    elaf_t apply_limits(atmsp::machine<fixp_t>& machine, account_name user, const rewardpool& pool, 
                        limits::kind_t kind, uint64_t cur_time, 
                        elaf_t w //it's abs_weight for vote and enable_vesting_payment-flag for post/comment
                        ) {
        eosio_assert((kind == limits::POST) || (kind == limits::COMM) || (kind == limits::VOTE), "forum::apply_limits: wrong act type");
        int64_t sum_vesting = eosio::vesting(vesting_name).get_account_effective_vesting(user, pool.state.funds.symbol.name()).amount;   
        if((kind == limits::VOTE) && (w != elaf_t(0))) {
            int64_t weighted_vesting = fp_cast<int64_t>(elai_t(sum_vesting) * w, false);
            eosio_assert(weighted_vesting >= pool.lims.get_min_vesting_for(kind), "forum::apply_limits: can't vote, not enough vesting");
        }
        else if(kind != limits::VOTE)
            eosio_assert(sum_vesting >= pool.lims.get_min_vesting_for(kind), "forum::apply_limits: can't post, not enough vesting");
        
        auto fp_vesting = fp_cast<fixp_t>(sum_vesting, false);
        
        charges chgs(_self, user);
        auto chgs_itr = get_itr(chgs, pool.created, _self, [&](auto &item) {item = {
                .poolcreated = pool.created,
                .vals = {pool.lims.get_charges_num(), {cur_time, 0}}
            };});
               
        auto cur_chgs = *chgs_itr;
        auto pre_chgs = cur_chgs;//we need it when POSTBW has no own charge
        auto power = cur_chgs.calc_power(machine, pool.lims, kind, cur_time, fp_vesting, (kind == limits::VOTE) ? w : elaf_t(1));
        
        auto ret = elaf_t(1);
        if(kind == limits::VOTE) {
             eosio_assert(power == 1, "forum::apply_limits: can't vote, not enough power");
             chgs.modify(chgs_itr, _self, [&](auto &item) { item = cur_chgs; });
        }
        else {
            if(power == 1)
                chgs.modify(chgs_itr, _self, [&](auto &item) { item = cur_chgs; });
            else if(w > elaf_t(0)) {//enable_vesting_payment
                int64_t user_vesting = eosio::vesting(vesting_name).get_account_unlocked_vesting(user, pool.state.funds.symbol.name()).amount;
                auto price = pool.lims.get_vesting_price(kind);
                eosio_assert(price > 0, "forum::apply_limits: can't vote, not enough power, vesting payment is disabled");
                eosio_assert(user_vesting >= price, "forum::apply_limits: insufficient vesting amount");
                INLINE_ACTION_SENDER(eosio::vesting, retire) (vesting_name, {_self, N(active)}, {_self, eosio::asset(price, pool.state.funds.symbol), user});
                cur_chgs = pre_chgs;
            }
            else
                eosio_assert(false, "forum::apply_limits: can't post, not enough power");
            ret = cur_chgs.calc_power(machine, pool.lims, limits::POSTBW, cur_time, fp_vesting);
        } 
        
        for(auto itr = chgs.begin(); itr != chgs.end();)
            if((cur_time - itr->poolcreated) / seconds(1).count() > MAX_CASHOUT_TIME)
                itr = chgs.erase(itr);
            else
                ++itr;
        return ret;
    }
    
public:
    using contract::contract; 
           
    [[eosio::action]]
    void setrules(const funcparams& mainfunc, const funcparams& curationfunc, const funcparams& timepenalty, 
        int64_t curatorsprop, int64_t maxtokenprop, eosio::symbol_type tokensymbol, limitsarg lims_arg) {
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
        atmsp::machine<fixp_t> machine;
        
        newrules.mainfunc     = load_func(mainfunc, "reward func", pa, machine, true);
        newrules.curationfunc = load_func(curationfunc, "curation func", pa, machine, true);
        newrules.timepenalty  = load_func(timepenalty, "time penalty func", pa, machine, true);
        
        newrules.curatorsprop = static_cast<base_t>(get_limit_prop(curatorsprop).data());
        newrules.maxtokenprop = static_cast<base_t>(get_limit_prop(maxtokenprop).data()); 
       
        limits lims{{}, {}, lims_arg.vestingprices, lims_arg.minvestings};
        for(auto& restorer_str : lims_arg.restorers)
            lims.restorers.emplace_back(load_restorer_func(restorer_str, pa, machine));
        for(auto& act : lims_arg.limitedacts)
            lims.limitedacts.emplace_back(limitedact{
                .chargenum   = act.chargenum, 
                .restorernum = act.restorernum, 
                .cutoffval   = get_prop(act.cutoffval).data(), 
                .chargeprice = get_prop(act.chargeprice).data()
            });
        lims.check();
              
        pools.emplace(_self, [&](auto &item) {
            item.created = created; 
            item.rules = newrules;
            item.lims = lims;
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
    void addmessage(account_name author, uint64_t id, int64_t tokenprop, 
                std::vector<beneficiary> beneficiaries
                //actually, beneficiaries[i].prop _here_ should be interpreted as (share * ONE_HUNDRED_PERCENT)
                //but not as a raw data for elaf_t, may be it's better to use another type (with uint16_t field for prop).
            , bool vestpayment) {
        require_auth(author);
        
        reward_pools pools(_self, _self);
        auto pool = pools.rbegin();
        eosio_assert(pool != pools.rend(), "forum::addmessage: [pools] is empty");
        check_account(author, pool->state.funds.symbol);
        
        std::map<account_name, int64_t> benefic_map;
        int64_t prop_sum = 0;
        for(auto& ben : beneficiaries) {
            check_account(ben.user, pool->state.funds.symbol);
            eosio_assert((0 <= ben.prop) && (ben.prop <= ONE_HUNDRED_PERCENT), "forum::addmessage: wrong ben.prop value");
            prop_sum += ben.prop;
            eosio_assert(prop_sum <= ONE_HUNDRED_PERCENT, "forum::addmessage: prop_sum > ONE_HUNDRED_PERCENT");
            benefic_map[ben.user] += ben.prop; //several entries for one user? ok.                     
        }
        eosio_assert((benefic_map.size() <= MAX_BENEFICIARIES), "forum::addmessage: benafic_map.size() > MAX_BENEFICIARIES");
        
        //reusing a vector
        beneficiaries.reserve(benefic_map.size());
        beneficiaries.clear();
        for(auto & ben : benefic_map)
            beneficiaries.emplace_back(beneficiary{
                .user = ben.first, 
                .prop = static_cast<base_t>(get_limit_prop(ben.second).data())
            });
           
        auto cur_time = current_time();

        atmsp::machine<fixp_t> machine;
        //TODO is it post or comment?
        auto reward_weight = apply_limits(machine, author, *pool, limits::POST, cur_time, vestpayment);     
       
        eosio_assert(cur_time >= pool->created, "forum::addmessage: cur_time < pool.created");
        eosio_assert(pool->state.msgs < std::numeric_limits<counter_t>::max(), "forum::addmessage: pool->msgs == max_counter_val");
       
        messages msgs(_self, author);
        
        eosio_assert(msgs.find(id) == msgs.end(), "forum::addmessage: id already exist");        
        
        pools.modify(*pool, _self, [&](auto &item){ item.state.msgs++; });
        msgs.emplace(_self, [&](auto &item) { item = {
            .id = id, 
            .created = cur_time, 
            .tokenprop = static_cast<base_t>(std::min(get_limit_prop(tokenprop), ELF(pool->rules.maxtokenprop)).data()),
            .beneficiaries = beneficiaries,
            .rewardweight = static_cast<base_t>(reward_weight.data())
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
        uint64_t cur_time = current_time();
        eosio_assert(cur_time >= msg->created, "forum::addvote: cur_time < msg->created");    

        reward_pools pools(_self, _self);
        auto pool = get_pool(pools, msg->created);        
        check_account(voter, pool->state.funds.symbol);
        
        elaf_t abs_w = get_limit_prop(abs(weight));
        //TODO abs_w == 0 only on revote
               
        atmsp::machine<fixp_t> machine;
        apply_limits(machine, voter, *pool, limits::VOTE, cur_time, abs_w);
        
        int64_t sum_vesting = eosio::vesting(vesting_name).get_account_effective_vesting(voter, pool->state.funds.symbol.name()).amount;        
        fixp_t abs_rshares = fp_cast<fixp_t>(sum_vesting, false) * abs_w;              
        fixp_t rshares = (weight < 0) ? -abs_rshares : abs_rshares;
        
        messagestate msg_new_state = {
            .absshares = add_cut(FP(msg->state.absshares), abs_rshares).data(),
            .netshares = add_cut(FP(msg->state.netshares), rshares).data(),
            .voteshares = ((rshares > fixp_t(0)) ? 
                add_cut(FP(msg->state.voteshares), rshares) :
                FP(msg->state.voteshares)).data()
            //.sumcuratorsw = see below
        };
        
        auto rsharesfn_delta = get_delta(machine, FP(msg->state.netshares), FP(msg_new_state.netshares), pool->rules.mainfunc);
                
        pools.modify(*pool, _self, [&](auto &item) {
             item.state.rshares = add_cut(FP(item.state.rshares), rshares).data();
             item.state.rsharesfn =  (FP(item.state.rsharesfn) + rsharesfn_delta).data();
        });
        
        auto sumcuratorsw_delta = get_delta(machine, FP(msg->state.voteshares), FP(msg_new_state.voteshares), pool->rules.curationfunc);
        msg_new_state.sumcuratorsw = (FP(msg->state.sumcuratorsw) + sumcuratorsw_delta).data();        
        msgs.modify(msg, _self, [&](auto &item) { item.state = msg_new_state; });     
        
        auto time_delta = static_cast<int64_t>((cur_time - msg->created) / seconds(1).count());
        auto curatorsw_factor = 
            std::max(std::min(
            set_and_run(machine, pool->rules.timepenalty.code, {fp_cast<fixp_t>(time_delta, false)}, {{fixp_t(0), FP(pool->rules.timepenalty.maxarg)}}),
            fixp_t(1)), fixp_t(0));
        //TODO: check if this vote already exist 
        vs.emplace(_self, [&](auto &item) {
            item = {
                .id = vs.available_primary_key(),    
                .msgid = msgid,
                .voter = voter,
                .weight = (fixp_t(sumcuratorsw_delta * curatorsw_factor)).data()
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
        atmsp::machine<fixp_t> machine;
        fixp_t sharesfn = set_and_run(machine, pool->rules.mainfunc.code, {FP(msg->state.netshares)}, {{fixp_t(0), FP(pool->rules.mainfunc.maxarg)}});
        
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

            payout = int_cast(ELF(msg->rewardweight) * elai_t(elai_t(state.funds.amount) * static_cast<elaf_t>(sharesfn / total_rsharesfn)));
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
               
        eosio_assert((curation_payout <= payout) && (curation_payout >= 0), "forum::payrewards: wrong curation_payout");
         
        auto unclaimed_rewards = pay_curators(author, id, curation_payout, FP(msg->state.sumcuratorsw), state.funds.symbol);
           
        eosio_assert(unclaimed_rewards >= 0, "forum::payrewards: unclaimed_rewards < 0");
           
        state.funds.amount += unclaimed_rewards;        
        payout -= curation_payout;
        
        int64_t ben_payout_sum = 0;
        for(auto& ben : msg->beneficiaries) {
            auto ben_payout = int_cast(elai_t(payout) * ELF(ben.prop));
            eosio_assert((0 <= ben_payout) && (ben_payout <= payout - ben_payout_sum), "LOGIC ERROR! forum::payrewards: wrong ben_payout value");
            payto(ben.user, eosio::asset(ben_payout, state.funds.symbol), static_cast<enum_t>(payment_t::VESTING));
            ben_payout_sum += ben_payout;
        }        
        payout -= ben_payout_sum;
        
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




 



