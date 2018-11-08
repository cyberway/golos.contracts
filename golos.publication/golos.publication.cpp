#include "golos.publication.hpp"
#include <common/hash64.hpp>
#include <eosiolib/transaction.hpp>
#include <eosiolib/types.hpp>
#include <eosio.token/eosio.token.hpp>
#include <golos.vesting/golos.vesting.hpp>

using namespace eosio;
using namespace golos::config;
using namespace atmsp::storable;

extern "C" {
    void apply(uint64_t receiver, uint64_t code, uint64_t action) {
        publication(receiver).apply(code, action);
    }
}

publication::publication(account_name self)
    : contract(self)
{}

void publication::apply(uint64_t code, uint64_t action) {
    if (N(transfer) == action && N(eosio.token) == code)
        execute_action(this, &publication::on_transfer);
    
    if (N(createmssg) == action)
        execute_action(this, &publication::create_message);
    if (N(updatemssg) == action)
        execute_action(this, &publication::update_message);
    if (N(deletemssg) == action)
        execute_action(this, &publication::delete_message);
    if (N(upvote) == action)
        execute_action(this, &publication::upvote);
    if (N(downvote) == action)
        execute_action(this, &publication::downvote);
    if (N(unvote) == action)
        execute_action(this, &publication::unvote);
    if (N(closemssg) == action)
        execute_action(this, &publication::close_message);
    if (N(createacc) == action)
        execute_action(this, &publication::create_acc);
    if (N(setrules) == action)
        execute_action(this, &publication::set_rules);
}

void publication::create_message(account_name account, std::string permlink,
                              account_name parentacc, std::string parentprmlnk,
                              std::vector<structures::beneficiary> beneficiaries,
                            //actually, beneficiaries[i].prop _here_ should be interpreted as (share * ONE_HUNDRED_PERCENT)
                            //but not as a raw data for elaf_t, may be it's better to use another type (with uint16_t field for prop).
                              int64_t tokenprop, bool vestpayment,
                              std::string headermssg,
                              std::string bodymssg, std::string languagemssg,
                              std::vector<structures::tag> tags,
                              std::string jsonmetadata) {
    require_auth(account);

    tables::reward_pools pools(_self, _self);
    auto pool = pools.begin();   // TODO: Reverse iterators doesn't work correctly
    eosio_assert(pool != pools.end(), "publication::create_message: [pools] is empty");
    pool = --pools.end();
    check_account(account, pool->state.funds.symbol);
    
    std::map<account_name, int64_t> benefic_map;
    int64_t prop_sum = 0;
    for(auto& ben : beneficiaries) {
        check_account(ben.account, pool->state.funds.symbol);
        eosio_assert((0 <= ben.deductprcnt) && (ben.deductprcnt <= ONE_HUNDRED_PERCENT), "publication::create_message: wrong ben.prop value");
        prop_sum += ben.deductprcnt;
        eosio_assert(prop_sum <= ONE_HUNDRED_PERCENT, "publication::create_message: prop_sum > ONE_HUNDRED_PERCENT");
        benefic_map[ben.account] += ben.deductprcnt; //several entries for one user? ok.                     
    }
    eosio_assert((benefic_map.size() <= MAX_BENEFICIARIES), "publication::create_message: benafic_map.size() > MAX_BENEFICIARIES");
    
    //reusing a vector
    beneficiaries.reserve(benefic_map.size());
    beneficiaries.clear();
    for(auto & ben : benefic_map)
        beneficiaries.emplace_back(structures::beneficiary{
            .account = ben.first,
            .deductprcnt = static_cast<base_t>(get_limit_prop(ben.second).data())
        });

    auto cur_time = current_time();
    
    atmsp::machine<fixp_t> machine;
    auto reward_weight = apply_limits(machine, account, *pool, 
        parentacc ? structures::limits::COMM : structures::limits::POST, cur_time, vestpayment); 

    eosio_assert(cur_time >= pool->created, "publication::create_message: cur_time < pool.created");
    eosio_assert(pool->state.msgs < std::numeric_limits<structures::counter_t>::max(), "publication::create_message: pool->msgs == max_counter_val"); 
    pools.modify(*pool, _self, [&](auto &item){ item.state.msgs++; });

    tables::message_table message_table(_self, account);
    auto message_id = fc::hash64(permlink);
    eosio_assert(message_table.find(message_id) == message_table.end(), "This message already exists.");           
    
    tables::content_table content_table(_self, account);
    auto parent_id = parentacc ? fc::hash64(parentprmlnk) : 0;
    
    uint8_t level = 0;
    if(parentacc)
        level = 1 + notify_parent(true, parentacc, parent_id);
    eosio_assert(level <= MAX_COMMENT_DEPTH, "publication::create_message: level > MAX_COMMENT_DEPTH");    
 
    message_table.emplace(account, [&]( auto &item ) {
        item.id = message_id;
        item.date = cur_time;
        item.parentacc = parentacc;
        item.parent_id = parent_id;
        item.tokenprop = static_cast<base_t>(std::min(get_limit_prop(tokenprop), ELF(pool->rules.maxtokenprop)).data()),
        item.beneficiaries = beneficiaries;
        item.rewardweight = static_cast<base_t>(reward_weight.data());
        item.childcount = 0;
        item.closed = false;
        item.level = level;
    });
    
   
    
    content_table.emplace(account, [&]( auto &item ) {
        item.id = message_id;
        item.headermssg = headermssg;
        item.bodymssg = bodymssg;
        item.languagemssg = languagemssg;
        item.tags = tags;
        item.jsonmetadata = jsonmetadata;
    });

    close_message_timer(account, message_id);
}

void publication::update_message(account_name account, std::string permlink,
                              std::string headermssg, std::string bodymssg,
                              std::string languagemssg, std::vector<structures::tag> tags,
                              std::string jsonmetadata) {
    require_auth(account);
    tables::content_table content_table(_self, account);
    auto cont_itr = content_table.find(fc::hash64(permlink));
    eosio_assert(cont_itr != content_table.end(), "Content doesn't exist.");          

    content_table.modify(cont_itr, account, [&]( auto &item ) {
        item.headermssg = headermssg;
        item.bodymssg = bodymssg;
        item.languagemssg = languagemssg;
        item.tags = tags;
        item.jsonmetadata = jsonmetadata;
    });
}

void publication::delete_message(account_name account, std::string permlink) {
    require_auth(account);

    tables::message_table message_table(_self, account);
    tables::content_table content_table(_self, account);
    tables::vote_table vote_table(_self, account);
    
    auto message_id = fc::hash64(permlink);
    auto mssg_itr = message_table.find(message_id);
    eosio_assert(mssg_itr != message_table.end(), "Message doesn't exist.");
    eosio_assert((mssg_itr->childcount) == 0, "You can't delete comment with child comments.");
    auto cont_itr = content_table.find(message_id);
    eosio_assert(cont_itr != content_table.end(), "Content doesn't exist.");    

    if(mssg_itr->parentacc)
        notify_parent(false, mssg_itr->parentacc, mssg_itr->parent_id);
    
    message_table.erase(mssg_itr);    
    content_table.erase(cont_itr);

    auto votetable_index = vote_table.get_index<N(messageid)>();
    auto vote_itr = votetable_index.lower_bound(message_id);
    while ((vote_itr != votetable_index.end()) && (vote_itr->message_id == message_id))
        vote_itr = votetable_index.erase(vote_itr);
}

void publication::upvote(account_name voter, account_name author, std::string permlink, int16_t weight) {
    require_auth(voter);
    eosio_assert(weight > 0, "The weight must be positive.");
    eosio_assert(weight <= ONE_HUNDRED_PERCENT, "The weight can't be more than 100%.");

    set_vote(voter, author, fc::hash64(permlink), weight);
}

void publication::downvote(account_name voter, account_name author, std::string permlink, int16_t weight) {
    require_auth(voter);

    eosio_assert(weight > 0, "The weight sign can't be negative.");
    eosio_assert(weight <= ONE_HUNDRED_PERCENT, "The weight can't be more than 100%.");

    set_vote(voter, author, fc::hash64(permlink), -weight);
}

void publication::unvote(account_name voter, account_name author, std::string permlink) {
    require_auth(voter);
  
    set_vote(voter, author, fc::hash64(permlink), 0);
}

void publication::payto(account_name user, eosio::asset quantity, enum_t mode) {
    require_auth(_self);
    eosio_assert(quantity.amount >= 0, "LOGIC ERROR! publication::payto: quantity.amount < 0");
    if(quantity.amount == 0)
        return;

    if(static_cast<payment_t>(mode) == payment_t::TOKEN)
        INLINE_ACTION_SENDER(eosio::token, transfer) (N(eosio.token), {_self, N(active)}, {_self, user, quantity, ""});
    else if(static_cast<payment_t>(mode) == payment_t::VESTING)
        INLINE_ACTION_SENDER(eosio::token, transfer) (N(eosio.token), {_self, N(active)}, {_self, vesting_name, quantity, name{user}.to_string()});
    else
        eosio_assert(false, "publication::payto: unknown kind of payment");
} 
 
int64_t publication::pay_curators(account_name author, uint64_t msgid, int64_t max_rewards, fixp_t weights_sum, eosio::symbol_type tokensymbol) {
    tables::vote_table vs(_self, author);
    int64_t unclaimed_rewards = max_rewards;
            
    auto idx = vs.get_index<N(messageid)>();
    auto v = idx.lower_bound(msgid);
    while ((v != idx.end()) && (v->message_id == msgid)) {
        if((weights_sum > fixp_t(0)) && (max_rewards > 0)) {
            auto claim = int_cast(elai_t(max_rewards) * elaf_t(FP(v->curatorsw) / weights_sum));
            eosio_assert(claim <= unclaimed_rewards, "LOGIC ERROR! publication::pay_curators: claim > unclaimed_rewards");
            if(claim > 0) {
                unclaimed_rewards -= claim;
                payto(v->voter, eosio::asset(claim, tokensymbol), static_cast<enum_t>(payment_t::VESTING));
            }
        }
        //v = idx.erase(v);
        ++v;
    }
    return unclaimed_rewards;
}

auto publication::get_pool(tables::reward_pools& pools, uint64_t time) {
    eosio_assert(pools.begin() != pools.end(), "publication::get_pool: [pools] is empty");
    
    auto pool = pools.upper_bound(time);
    
    eosio_assert(pool != pools.begin(), "publication::get_pool: can't find an appropriate pool");        
    return (--pool);
}

void publication::close_message(account_name account, uint64_t id) {
    require_auth(_self);
    tables::message_table message_table(_self, account);
    auto mssg_itr = message_table.find(id);
    eosio_assert(mssg_itr != message_table.end(), "Message doesn't exist.");
    eosio_assert(!mssg_itr->closed, "Message is already closed.");
    
    tables::reward_pools pools(_self, _self);
    auto pool = get_pool(pools, mssg_itr->date);     

    eosio_assert(pool->state.msgs != 0, "LOGIC ERROR! publication::payrewards: pool.msgs is equal to zero");
    atmsp::machine<fixp_t> machine;
    fixp_t sharesfn = set_and_run(machine, pool->rules.mainfunc.code, {FP(mssg_itr->state.netshares)}, {{fixp_t(0), FP(pool->rules.mainfunc.maxarg)}});
    
    auto state = pool->state;
    
    int64_t payout = 0;
    if(state.msgs == 1) {
        payout = state.funds.amount;
        eosio_assert(state.rshares == mssg_itr->state.netshares, "LOGIC ERROR! publication::payrewards: pool->rshares != mssg_itr->netshares for last message"); 
        eosio_assert(state.rsharesfn == sharesfn.data(), "LOGIC ERROR! publication::payrewards: pool->rsharesfn != sharesfn.data() for last message"); 
        state.funds.amount = 0;
        state.rshares = 0; 
        state.rsharesfn = 0;
    } 
    else {
        auto total_rsharesfn = FP(state.rsharesfn);
        
        if(sharesfn > fixp_t(0)) {
            eosio_assert(total_rsharesfn > fixp_t(0), "LOGIC ERROR! publication::payrewards: total_rshares_fn <= 0"); 
            payout = int_cast(ELF(mssg_itr->rewardweight) * elai_t(elai_t(state.funds.amount) * static_cast<elaf_t>(sharesfn / total_rsharesfn)));
            state.funds.amount -= payout;
            eosio_assert(state.funds.amount >= 0, "LOGIC ERROR! publication::payrewards: state.funds < 0"); 
        }
        
        auto new_rshares = FP(state.rshares) - FP(mssg_itr->state.netshares);
        auto new_rsharesfn = FP(state.rsharesfn) - sharesfn;
        
        eosio_assert(new_rshares >= fixp_t(0), "LOGIC ERROR! publication::payrewards: new_rshares < 0"); 
        eosio_assert(new_rsharesfn >= fixp_t(0), "LOGIC ERROR! publication::payrewards: new_rsharesfn < 0"); 
        
        state.rshares = new_rshares.data();
        state.rsharesfn = new_rsharesfn.data();            
    }
    
    auto curation_payout = int_cast(ELF(pool->rules.curatorsprop) * elai_t(payout));
           
    eosio_assert((curation_payout <= payout) && (curation_payout >= 0), "publication::payrewards: wrong curation_payout");
     
    auto unclaimed_rewards = pay_curators(account, id, curation_payout, FP(mssg_itr->state.sumcuratorsw), state.funds.symbol);
       
    eosio_assert(unclaimed_rewards >= 0, "publication::payrewards: unclaimed_rewards < 0");
       
    state.funds.amount += unclaimed_rewards;        
    payout -= curation_payout;
    
    int64_t ben_payout_sum = 0;
    for(auto& ben : mssg_itr->beneficiaries) {
        auto ben_payout = int_cast(elai_t(payout) * ELF(ben.deductprcnt));
        eosio_assert((0 <= ben_payout) && (ben_payout <= payout - ben_payout_sum), "LOGIC ERROR! publication::payrewards: wrong ben_payout value");
        payto(ben.account, eosio::asset(ben_payout, state.funds.symbol), static_cast<enum_t>(payment_t::VESTING));
        ben_payout_sum += ben_payout;
    }        
    payout -= ben_payout_sum;
    
    auto token_payout = int_cast(elai_t(payout) * ELF(mssg_itr->tokenprop));
    eosio_assert(payout >= token_payout, "publication::payrewards: wrong token_payout value");
    
    payto(account, eosio::asset(token_payout, state.funds.symbol), static_cast<enum_t>(payment_t::TOKEN));
    payto(account, eosio::asset(payout - token_payout, state.funds.symbol), static_cast<enum_t>(payment_t::VESTING));
     
    //message_table.erase(mssg_itr);
    message_table.modify(mssg_itr, _self, [&]( auto &item) {
        item.closed = true;
    });
    
    bool pool_erased = false;
    state.msgs--;      
    if(state.msgs == 0) {
        if(pool != --pools.end()) {//there is a pool after, so we can delete this one        
            eosio_assert(state.funds.amount == unclaimed_rewards, "LOGIC ERROR! publication::payrewards: state.funds != unclaimed_rewards");                
            fill_depleted_pool(pools, state.funds, pool);
            pools.erase(pool);
            pool_erased = true;             
        }
    }        
    if(!pool_erased)
        pools.modify(pool, _self, [&](auto &item) { item.state = state; });
}

void publication::close_message_timer(account_name account, uint64_t id) {
    transaction trx;
    trx.actions.emplace_back(action{permission_level(_self, N(active)), _self, N(closemssg), structures::accandvalue{account, id}});
    trx.delay_sec = CLOSE_MESSAGE_PERIOD;
    trx.send((static_cast<uint128_t>(id) << 64) | account, _self);
}

void publication::set_vote(account_name voter, account_name author, uint64_t id, int16_t weight) {
    tables::message_table message_table(_self, author);
    auto mssg_itr = message_table.find(id);
    eosio_assert(mssg_itr != message_table.end(), "Message doesn't exist.");
    tables::reward_pools pools(_self, _self);
    auto pool = get_pool(pools, mssg_itr->date); 
    check_account(voter, pool->state.funds.symbol);
    tables::vote_table vote_table(_self, author);
    
    auto cur_time = current_time();

    eosio_assert((cur_time <= mssg_itr->date + ((CLOSE_MESSAGE_PERIOD - UPVOTE_DISABLE_PERIOD) * seconds(1).count())) ||
                 (cur_time > mssg_itr->date + (CLOSE_MESSAGE_PERIOD * seconds(1).count())) ||
                 (weight <= 0),
                  "You can't upvote, because publication will be closed soon.");

    auto votetable_index = vote_table.get_index<N(messageid)>();
    auto vote_itr = votetable_index.lower_bound(id);
    while ((vote_itr != votetable_index.end()) && (vote_itr->message_id == id)) {
        if (voter == vote_itr->voter) {
            // it's not consensus part and can be moved to storage in future
            if (mssg_itr->closed) {
                votetable_index.modify(vote_itr, voter, [&]( auto &item ) {
                    item.count = -1;
                });
                return;
            }
            //TODO: recalc: pool.rshares message.rshares; message.sumcuratorsw -= curatorw; curatorw := 0;
            eosio_assert(weight != vote_itr->weight, "Vote with the same weight has already existed.");
            eosio_assert(vote_itr->count != MAX_REVOTES, "You can't revote anymore.");
            votetable_index.modify(vote_itr, voter, [&]( auto &item ) {
               item.weight = weight;
               item.time = cur_time;
               ++item.count;
            });
            return;
        }
        ++vote_itr;
    }

    elaf_t abs_w = get_limit_prop(abs(weight));
    atmsp::machine<fixp_t> machine;
    apply_limits(machine, voter, *pool, structures::limits::VOTE, cur_time, abs_w);

    int64_t sum_vesting = golos::vesting(vesting_name).get_account_effective_vesting(voter, pool->state.funds.symbol.name()).amount;
    fixp_t abs_rshares = fp_cast<fixp_t>(sum_vesting, false) * abs_w;
    fixp_t rshares = (weight < 0) ? -abs_rshares : abs_rshares;
    
    structures::messagestate msg_new_state = {
        .absshares = add_cut(FP(mssg_itr->state.absshares), abs_rshares).data(),
        .netshares = add_cut(FP(mssg_itr->state.netshares), rshares).data(),
        .voteshares = ((rshares > fixp_t(0)) ? 
            add_cut(FP(mssg_itr->state.voteshares), rshares) :
            FP(mssg_itr->state.voteshares)).data()
        //.sumcuratorsw = see below
    };
    
    auto rsharesfn_delta = get_delta(machine, FP(mssg_itr->state.netshares), FP(msg_new_state.netshares), pool->rules.mainfunc);
   
    pools.modify(*pool, _self, [&](auto &item) {
         item.state.rshares = add_cut(FP(item.state.rshares), rshares).data();
         item.state.rsharesfn =  (FP(item.state.rsharesfn) + rsharesfn_delta).data();
    });   
    
    auto sumcuratorsw_delta = get_delta(machine, FP(mssg_itr->state.voteshares), FP(msg_new_state.voteshares), pool->rules.curationfunc);
    msg_new_state.sumcuratorsw = (FP(mssg_itr->state.sumcuratorsw) + sumcuratorsw_delta).data();        
    message_table.modify(mssg_itr, _self, [&](auto &item) { item.state = msg_new_state; });  
    
    auto time_delta = static_cast<int64_t>((cur_time - mssg_itr->date) / seconds(1).count());
    auto curatorsw_factor = 
        std::max(std::min(
        set_and_run(machine, pool->rules.timepenalty.code, {fp_cast<fixp_t>(time_delta, false)}, {{fixp_t(0), FP(pool->rules.timepenalty.maxarg)}}),
        fixp_t(1)), fixp_t(0));   
    
    vote_table.emplace(voter, [&]( auto &item ) {
        item.id = vote_table.available_primary_key();
        item.message_id = mssg_itr->id;
        item.voter = voter;
        item.weight = weight;
        item.time = cur_time;
        item.count = mssg_itr->closed ? -1 : (item.count + 1);
        item.curatorsw = (fixp_t(sumcuratorsw_delta * curatorsw_factor)).data();   
    });
}

uint16_t publication::notify_parent(bool increase, account_name parentacc, uint64_t parent_id) {
    tables::message_table message_table(_self, parentacc);
    auto mssg_itr = message_table.find(parent_id);
    eosio_assert(mssg_itr != message_table.end(), "Parent message doesn't exist");
    message_table.modify(mssg_itr, 0, [&]( auto &item ) {
        if (increase)
            ++item.childcount;
        else
            --item.childcount;
    });
    return mssg_itr->level;
}

void publication::fill_depleted_pool(tables::reward_pools& pools, eosio::asset quantity, tables::reward_pools::const_iterator excluded) {
    using namespace tables;
    using namespace structures;
    eosio_assert(quantity.amount >= 0, "fill_depleted_pool: quantity.amount < 0");
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

void publication::on_transfer(account_name from, account_name to, eosio::asset quantity, std::string memo) {        
    (void)memo;
    require_auth(from);  
    if(_self != to)
        return;  

    tables::reward_pools pools(_self, _self); 
    fill_depleted_pool(pools, quantity, pools.end()); 
}

void publication::set_rules(const funcparams& mainfunc, const funcparams& curationfunc, const funcparams& timepenalty, 
    int64_t curatorsprop, int64_t maxtokenprop, eosio::symbol_type tokensymbol, limitsarg lims_arg) {
    //TODO: machine's constants
    using namespace tables;
    using namespace structures;
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

structures::funcinfo publication::load_func(const funcparams& params, const std::string& name, const atmsp::parser<fixp_t>& pa, atmsp::machine<fixp_t>& machine, bool inc) {
    eosio_assert(params.maxarg > 0, "forum::load_func: params.maxarg <= 0");
    structures::funcinfo ret;     
    ret.maxarg = fp_cast<fixp_t>(params.maxarg).data();
    pa(machine, params.str, "x");
    check_positive_monotonic(machine, FP(ret.maxarg), name, inc);
    ret.code.from_machine(machine);
    return ret;
}

bytecode publication::load_restorer_func(const std::string str_func, const atmsp::parser<fixp_t>& pa, atmsp::machine<fixp_t>& machine) {
    bytecode ret;
    pa(machine, str_func, "p,v,t");//prev value, vesting, time(elapsed seconds)
    ret.from_machine(machine);
    return ret;
}

fixp_t publication::get_delta(atmsp::machine<fixp_t>& machine, fixp_t old_val, fixp_t new_val, const structures::funcinfo& func) {
    func.code.to_machine(machine);
    elap_t old_fn = machine.run({old_val}, {{fixp_t(0), FP(func.maxarg)}});
    elap_t new_fn = machine.run({new_val}, {{fixp_t(0), FP(func.maxarg)}});
    return fp_cast<fixp_t>(new_fn - old_fn, false);
}

void publication::check_account(account_name user, eosio::symbol_type tokensymbol) {
    eosio_assert(golos::vesting(vesting_name).balance_exist(user, tokensymbol.name()), ("unregistered user: " + name{user}.to_string()).c_str());
}

elaf_t publication::apply_limits(atmsp::machine<fixp_t>& machine, account_name user, 
                    const structures::rewardpool& pool, 
                    structures::limits::kind_t kind, uint64_t cur_time, 
                    elaf_t w //it's abs_weight for vote and enable_vesting_payment-flag for post/comment
                    ) {
    using namespace tables;
    using namespace structures;
    eosio_assert((kind == limits::POST) || (kind == limits::COMM) || (kind == limits::VOTE), "publication::apply_limits: wrong act type");
    int64_t sum_vesting = golos::vesting(vesting_name).get_account_effective_vesting(user, pool.state.funds.symbol.name()).amount;
    if((kind == limits::VOTE) && (w != elaf_t(0))) {
        int64_t weighted_vesting = fp_cast<int64_t>(elai_t(sum_vesting) * w, false);
        eosio_assert(weighted_vesting >= pool.lims.get_min_vesting_for(kind), "publication::apply_limits: can't vote, not enough vesting");
    }
    else if(kind != limits::VOTE)
        eosio_assert(sum_vesting >= pool.lims.get_min_vesting_for(kind), "publication::apply_limits: can't post, not enough vesting");
    
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
         eosio_assert(power == 1, "publication::apply_limits: can't vote, not enough power");
         chgs.modify(chgs_itr, _self, [&](auto &item) { item = cur_chgs; });
    }
    else {
        if(power == 1)
            chgs.modify(chgs_itr, _self, [&](auto &item) { item = cur_chgs; });
        else if(w > elaf_t(0)) {//enable_vesting_payment
            int64_t user_vesting = golos::vesting(vesting_name).get_account_unlocked_vesting(user, pool.state.funds.symbol.name()).amount;
            auto price = pool.lims.get_vesting_price(kind);
            eosio_assert(price > 0, "publication::apply_limits: can't post, not enough power, vesting payment is disabled");
            eosio_assert(user_vesting >= price, "publication::apply_limits: insufficient vesting amount");
            INLINE_ACTION_SENDER(golos::vesting, retire) (vesting_name, {_self, N(active)}, {_self, eosio::asset(price, pool.state.funds.symbol), user});
            cur_chgs = pre_chgs;
        }
        else
            eosio_assert(false, "publication::apply_limits: can't post, not enough power");
        ret = cur_chgs.calc_power(machine, pool.lims, limits::POSTBW, cur_time, fp_vesting);
    } 
    
    for(auto itr = chgs.begin(); itr != chgs.end();)
        if((cur_time - itr->poolcreated) / seconds(1).count() > MAX_CASHOUT_TIME)
            itr = chgs.erase(itr);
        else
            ++itr;
    return ret;
}

void publication::create_acc(account_name name) { //DUMMY 
}

