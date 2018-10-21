#include "golos.publication.hpp"
#include <common/hash64.hpp>
#include <eosiolib/transaction.hpp>
#include <eosiolib/types.hpp>
#include <eosio.token/eosio.token.hpp>

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
                              std::string headermssg,
                              std::string bodymssg, std::string languagemssg,
                              std::vector<structures::tag> tags,
                              std::string jsonmetadata) {
    require_auth(account);

    tables::message_table message_table(_self, account);
    auto message_id = fc::hash64(permlink);
    eosio_assert(message_table.find(message_id) == message_table.end(), "This message already exists.");           
    
    tables::content_table content_table(_self, account);
    auto parent_id = parentacc ? fc::hash64(parentprmlnk) : 0;
 
    message_table.emplace(account, [&]( auto &item ) {
        item.id = message_id;
        item.date = now();
        item.parentacc = parentacc;
        item.parent_id = parent_id;
        item.beneficiaries = beneficiaries;
        item.childcount = 0;
        item.closed = false;
    });
    
    if(parentacc)
        set_child_count(true, parentacc, parent_id);        
    
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
        set_child_count(false, mssg_itr->parentacc, mssg_itr->parent_id);
    
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

void publication::close_message(account_name account, uint64_t id) {
    require_auth(_self);
    tables::message_table message_table(_self, account);
    auto mssg_itr = message_table.find(id);
    eosio_assert(mssg_itr != message_table.end(), "Message doesn't exist.");
    
    message_table.modify(mssg_itr, _self, [&]( auto &item) {
        item.closed = true;
    });
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
    tables::vote_table vote_table(_self, author);

    eosio_assert((now() <= mssg_itr->date + CLOSE_MESSAGE_PERIOD - UPVOTE_DISABLE_PERIOD) ||
                 (now() > mssg_itr->date + CLOSE_MESSAGE_PERIOD) ||
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
            // end not consensus
            eosio_assert(weight != vote_itr->weight, "Vote with the same weight has already existed.");
            eosio_assert(vote_itr->count != MAX_REVOTES, "You can't revote anymore.");
            votetable_index.modify(vote_itr, voter, [&]( auto &item ) {
               item.weight = weight;
               item.time = now();
               ++item.count;
            });
            return;
        }
        ++vote_itr;
    }
    
    vote_table.emplace(voter, [&]( auto &item ) {
        item.id = vote_table.available_primary_key();
        item.message_id = mssg_itr->id;
        item.voter = voter;
        item.weight = weight;
        item.time = now();
        item.count = mssg_itr->closed ? -1 : (item.count + 1);
    });
}

void publication::set_child_count(bool increase, account_name parentacc, uint64_t parent_id) {
    tables::message_table message_table(_self, parentacc);
    auto mssg_itr = message_table.find(parent_id);
    eosio_assert(mssg_itr != message_table.end(), "Parent message doesn't exist");
    message_table.modify(mssg_itr, 0, [&]( auto &item ) {
        if (increase)
            ++item.childcount;
        else
            --item.childcount;
    }); 
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


void publication::create_acc(account_name name) { //DUMMY 
}

