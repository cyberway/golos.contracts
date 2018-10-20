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
    if (N(createpost) == action)
        execute_action(this, &publication::create_post);
    if (N(updatepost) == action)
        execute_action(this, &publication::update_post);
    if (N(deletepost) == action)
        execute_action(this, &publication::delete_post);
    if (N(upvote) == action)
        execute_action(this, &publication::upvote);
    if (N(downvote) == action)
        execute_action(this, &publication::downvote);
    if (N(unvote) == action)
        execute_action(this, &publication::unvote);
    if (N(closepost) == action)
        execute_action(this, &publication::close_post);
    if (N(createacc) == action)
        execute_action(this, &publication::create_acc);
}

void publication::create_post(account_name account, std::string permlink,
                              account_name parentacc, std::string parentprmlnk,
                              std::vector<structures::beneficiary> beneficiaries,
                              std::string headerpost,
                              std::string bodypost, std::string languagepost,
                              std::vector<structures::tag> tags,
                              std::string jsonmetadata) {
    require_auth(account);

    tables::post_table post_table(_self, account);
    auto post_id = fc::hash64(permlink);
    eosio_assert(post_table.find(post_id) == post_table.end(), "This post already exists.");           
    
    tables::content_table content_table(_self, account);
    auto parent_id = parentacc ? fc::hash64(parentprmlnk) : 0;
 
    post_table.emplace(account, [&]( auto &item ) {
        item.id = post_id;
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
        item.id = post_id;
        item.headerpost = headerpost;
        item.bodypost = bodypost;
        item.languagepost = languagepost;
        item.tags = tags;
        item.jsonmetadata = jsonmetadata;
    });

    close_post_timer(account, post_id);
}

void publication::update_post(account_name account, std::string permlink,
                              std::string headerpost, std::string bodypost,
                              std::string languagepost, std::vector<structures::tag> tags,
                              std::string jsonmetadata) {
    require_auth(account);
    tables::content_table content_table(_self, account);
    auto cont_itr = content_table.find(fc::hash64(permlink));
    eosio_assert(cont_itr != content_table.end(), "Content doesn't exist.");          

    content_table.modify(cont_itr, account, [&]( auto &item ) {
        item.headerpost = headerpost;
        item.bodypost = bodypost;
        item.languagepost = languagepost;
        item.tags = tags;
        item.jsonmetadata = jsonmetadata;
    });
}

void publication::delete_post(account_name account, std::string permlink) {
    require_auth(account);

    tables::post_table post_table(_self, account);
    tables::content_table content_table(_self, account);
    tables::vote_table vote_table(_self, account);
    
    auto post_id = fc::hash64(permlink);
    auto post_itr = post_table.find(post_id);
    eosio_assert(post_itr != post_table.end(), "Post doesn't exist.");
    eosio_assert((post_itr->childcount) == 0, "You can't delete comment with child comments.");
    auto cont_itr = content_table.find(post_id);
    eosio_assert(cont_itr != content_table.end(), "Content doesn't exist.");    

    if(post_itr->parentacc)
        set_child_count(false, post_itr->parentacc, post_itr->parent_id);
    
    post_table.erase(post_itr);    
    content_table.erase(cont_itr);
    
    auto votetable_index = vote_table.get_index<N(postid)>();
    auto votetable_obj = votetable_index.find(post_id);
    while (votetable_obj != votetable_index.end())
        votetable_obj = votetable_index.erase(votetable_obj);
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

void publication::close_post(account_name account, uint64_t id) {
    require_auth(_self);
    tables::post_table post_table(_self, account);
    auto post_itr = post_table.find(id);
    eosio_assert(post_itr != post_table.end(), "Post doesn't exist.");
    
    post_table.modify(post_itr, _self, [&]( auto &item) {
        item.closed = true;
    });
}

void publication::close_post_timer(account_name account, uint64_t id) {
    transaction trx;
    trx.actions.emplace_back(action{permission_level(_self, N(active)), _self, N(closepost), structures::accandvalue{account, id}});
    trx.delay_sec = CLOSE_POST_PERIOD;
    trx.send((static_cast<uint128_t>(id) << 64) | account, _self);
}

void publication::set_vote(account_name voter, account_name author, uint64_t id, int16_t weight) {
    tables::post_table post_table(_self, author);
    auto post_itr = post_table.find(id);
    eosio_assert(post_itr != post_table.end(), "Post doesn't exist.");
    tables::vote_table vote_table(_self, author);

    eosio_assert((now() <= post_itr->date + CLOSE_POST_PERIOD - UPVOTE_DISABLE_PERIOD) ||
                 (now() > post_itr->date + CLOSE_POST_PERIOD),
                  "You can't upvote, because publication will be closed soon.");

    auto votetable_index = vote_table.get_index<N(postid)>();
    auto votetable_obj = votetable_index.find(id);         

    while (votetable_obj != votetable_index.end()) {
        if (voter == votetable_obj->voter) {
            // it's not consensus part and can be moved to storage in future
            if (post_itr->closed) {
                votetable_index.modify(votetable_obj, voter, [&]( auto &item ) {
                    item.count = -1;
                });
                return;
            }
            // end not consensus
            eosio_assert(weight != votetable_obj->weight, "Vote with the same weight has already existed.");
            eosio_assert(votetable_obj->count != MAX_REVOTES, "You can't revote anymore.");
            votetable_index.modify(votetable_obj, voter, [&]( auto &item ) {
               item.weight = weight;
               item.time = now();
               ++item.count;
            });
            return;
        }
        ++votetable_obj;
    }
    
    vote_table.emplace(voter, [&]( auto &item ) {
        item.id = vote_table.available_primary_key();
        item.post_id = post_itr->id;
        item.voter = voter;
        item.weight = weight;
        item.time = now();
        item.count = post_itr->closed ? -1 : (item.count + 1);
    });
}

void publication::set_child_count(bool increase, account_name parentacc, uint64_t parent_id) {
    tables::post_table post_table(_self, parentacc);
    auto itr = post_table.find(parent_id);
    eosio_assert(itr != post_table.end(), "Parent post doesn't exist");
    post_table.modify(itr, 0, [&]( auto &item ) {
        if (increase)
            ++item.childcount;
        else
            --item.childcount;
    }); 
}

void publication::create_acc(account_name name) { //DUMMY 
}

