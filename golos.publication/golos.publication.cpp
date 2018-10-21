#include "golos.publication.hpp"
#include "config.hpp"
#include <common/hash64.hpp>
#include <eosiolib/transaction.hpp>
#include <eosiolib/types.hpp>

using namespace eosio;

extern "C" {
    void apply(uint64_t receiver, uint64_t code, uint64_t action) {
        publication(receiver).apply(code, action);
    }
}

publication::publication(account_name self)
    : contract(self)
{}

void publication::apply(uint64_t code, uint64_t action) {
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
    eosio_assert(weight <= MAX_WEIGHT, "The weight can't be more than 100%.");

    set_vote(voter, author, fc::hash64(permlink), weight);
}

void publication::downvote(account_name voter, account_name author, std::string permlink, int16_t weight) {
    require_auth(voter);

    eosio_assert(weight > 0, "The weight sign can't be negative.");
    eosio_assert(weight <= MAX_WEIGHT, "The weight can't be more than 100%.");

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

void publication::create_acc(account_name name) { //DUMMY 
}

