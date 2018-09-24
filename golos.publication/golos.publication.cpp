#include "golos.publication.hpp"
#include "config.hpp"

#include <eosiolib/transaction.hpp>

using namespace eosio;

extern "C" {
    void apply(uint64_t receiver, uint64_t code, uint64_t action) {
        publication(receiver).apply(code, action);
    }
}

publication::publication(account_name self)
    : contract(self)
    , _post_table(_self, _self)
    , _content_table(_self, _self)
    , _vote_table(_self, _self)
    , _voters_table(_self, _self)
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
    if (N(closepost) == action)
        close_post();
}

void publication::create_post(account_name account, std::string permlink,
                              account_name parentacc, std::string parentprmlnk,
                              uint64_t curatorprcnt, std::string payouttype,
                              std::vector<structures::beneficiary> beneficiaries,
                              std::string paytype, std::string headerpost,
                              std::string bodypost, std::string languagepost,
                              std::vector<structures::tag> tags,
                              std::string jsonmetadata) {

    require_auth(account);

    for (auto posttable_obj = _post_table.begin(); posttable_obj != _post_table.end(); ++posttable_obj)
        eosio_assert(posttable_obj->account != account && posttable_obj->permlink != permlink,
                     "This post already exists.");

    _post_table.emplace(account, [&]( auto &item ) {
        item.id = _post_table.available_primary_key();
        item.date = now();
        item.account = account;
        item.permlink = permlink;
        item.parentacc = parentacc;
        item.parentprmlnk = parentprmlnk;
        item.curatorprcnt = curatorprcnt;
        item.payouttype = payouttype;
        item.beneficiaries = beneficiaries;
        item.paytype = paytype;
    });

    _content_table.emplace(account, [&]( auto &item ) {
        item.id = _content_table.available_primary_key();
        item.headerpost = headerpost;
        item.bodypost = bodypost;
        item.languagepost = languagepost;
        item.tags = tags;
        item.jsonmetadata = jsonmetadata;
    });
}

void publication::update_post(account_name account, std::string permlink,
                              account_name parentacc, std::string parentprmlnk,
                              uint64_t curatorprcnt, std::string payouttype,
                              std::vector<structures::beneficiary> beneficiaries,
                              std::string paytype, std::string headerpost,
                              std::string bodypost, std::string languagepost,
                              std::vector<structures::tag> tags,
                              std::string jsonmetadata) {
    require_auth(account);

    for (auto posttable_obj = _post_table.begin(); posttable_obj != _post_table.end(); ++posttable_obj) {
        if (posttable_obj->account == account && posttable_obj->permlink == permlink) {
            eosio_assert(posttable_obj->parentacc == parentacc, "Parent account was changed.");
            eosio_assert(posttable_obj->parentprmlnk == parentprmlnk, "Parent permlink was changed.");
            eosio_assert(posttable_obj->curatorprcnt == curatorprcnt, "Curator percent was changed.");
            eosio_assert(posttable_obj->payouttype == payouttype, "Payout type was changed.");
            for (auto table_beneficiary : posttable_obj->beneficiaries)
                for (auto param_beneficiary : beneficiaries)
                    eosio_assert(table_beneficiary.account == param_beneficiary.account, "Beneficiaries was changed.");
            eosio_assert(posttable_obj->paytype == paytype, "Pay type was changed.");

            auto contenttable_index = _content_table.get_index<N(id)>();
            auto contenttable_obj = contenttable_index.find(posttable_obj->id);

            contenttable_index.modify(contenttable_obj, account, [&]( auto &item ) {
                item.headerpost = headerpost;
                item.bodypost = bodypost;
                item.languagepost = languagepost;
                item.tags = tags;
                item.jsonmetadata = jsonmetadata;
            });
            break;
        } else {
            eosio_assert(false, "Post doesn't exist.");
        }
    }
}

void publication::delete_post(account_name account, std::string permlink) {
    require_auth(account);

    for (auto posttable_obj = _post_table.begin(); posttable_obj != _post_table.end(); ++posttable_obj) {
        eosio_assert(posttable_obj != _post_table.end(), "Post doesn't exist.");
        if (posttable_obj->account == account && posttable_obj->permlink == permlink) {            
            if (posttable_obj->parentacc != std::stoi("")) { // check
                for (auto obj = _post_table.begin(); obj != _post_table.end(); ++obj) {              
                    if (obj->parentacc != account && obj->parentprmlnk != permlink) {                       
                        _post_table.erase(posttable_obj);
                        auto contenttable_index = _content_table.get_index<N(id)>();
                        auto contenttable_obj = contenttable_index.find(posttable_obj->id);
                        contenttable_index.erase(contenttable_obj);
                        auto votetable_index = _vote_table.get_index<N(post_id)>();
                        auto votetable_obj = votetable_index.find(posttable_obj->id);
                        votetable_index.erase(votetable_obj);
                        for (auto voterstable_obj = _voters_table.begin(); voterstable_obj != _voters_table.end(); )
                            if (posttable_obj->id == voterstable_obj->post_id) {
                                voterstable_obj = _voters_table.erase(voterstable_obj);
                            } else {
                                ++voterstable_obj;
                            }
                        break;
                    } else {
                        eosio_assert(false, "You can't delete comment with child comments.");
                    }
                }
                break;
            } else {
                eosio_assert(false, "You can't delete post, it can be only changed.");
            }
        }
    }
}

void publication::upvote(account_name voter, account_name author, std::string permlink, asset weight) {
    eosio::print("\nupvote begin");

    require_auth(voter);

    std::vector<structures::rshares> rshares;

    structures::rshares rshares_obj;

    rshares_obj.net_rshares = 0;
    rshares_obj.abs_rshares = 0;
    rshares_obj.vote_rshares = 0;
    rshares_obj.children_abs_rshares = 0;

    rshares.push_back(rshares_obj);

    for (auto posttable_obj = _post_table.begin(); posttable_obj != _post_table.end(); ++posttable_obj) {
        eosio_assert(posttable_obj != _post_table.end(), "Post doesn't exist.");
        if (posttable_obj->account == author && posttable_obj->permlink == permlink) {
            auto votetable_index = _vote_table.get_index<N(post_id)>();
            auto votetable_obj = votetable_index.find(posttable_obj->id);
            if (votetable_obj != votetable_index.end()) {
                for (auto voterstable_obj = _voters_table.begin(); voterstable_obj != _voters_table.end(); ++voterstable_obj) {
                    if (posttable_obj->id == voterstable_obj->post_id && voter == voterstable_obj->voter) {
                        eosio_assert(false, "You have already voted for this post.");
                    } else {
                        _voters_table.emplace(voter, [&]( auto &item ) {
                            item.id = _voters_table.available_primary_key();
                            item.post_id = posttable_obj->id;
                            item.voter = voter;
                        });

                        votetable_index.modify(votetable_obj, voter, [&]( auto &item ) {
                           item.post_id = posttable_obj->id;
                           item.voter = voter;
                           item.percent = FIXED_CURATOR_PERCENT;
                           item.weight = weight;
                           item.time = now();
                           item.rshares = rshares;
                           item.count++;
                        });
                        break;
                    }
                }
            } else {
                _voters_table.emplace(voter, [&]( auto &item ) {
                    item.id = _voters_table.available_primary_key();
                    item.post_id = posttable_obj->id;
                    item.voter = voter;
                });

                _vote_table.emplace(voter, [&]( auto &item ) {
                   item.post_id = posttable_obj->id;
                   item.voter = voter;
                   item.percent = FIXED_CURATOR_PERCENT;
                   item.weight = weight;
                   item.time = now();
                   item.rshares = rshares;
                   item.count++;
                });
            }
        }
    }

    eosio::print("\nupvote end");
}

void publication::downvote(account_name voter, account_name author, std::string permlink, asset weight) {
    eosio::print("\ndownvote begin");

    require_auth(voter);

    std::vector<structures::rshares> rshares;

    structures::rshares rshares_obj;

    rshares_obj.net_rshares = 0;
    rshares_obj.abs_rshares = 0;
    rshares_obj.vote_rshares = 0;
    rshares_obj.children_abs_rshares = 0;

    rshares.push_back(rshares_obj);

    for (auto posttable_obj = _post_table.begin(); posttable_obj != _post_table.end(); ++posttable_obj) {
        if (posttable_obj->account == author && posttable_obj->permlink == permlink) {
            for (auto voterstable_obj = _voters_table.begin(); voterstable_obj != _voters_table.end(); ++voterstable_obj) {
                if (posttable_obj->id == voterstable_obj->post_id && voter == voterstable_obj->voter) {
                    _voters_table.erase(voterstable_obj);
                    break;
                }
                eosio_assert(voterstable_obj != _voters_table.end(),
                             "You can't do down vote because previously you haven't voted for this post.");
            }
            auto votetable_index = _vote_table.get_index<N(post_id)>();
            auto votetable_obj = votetable_index.find(posttable_obj->id);

            votetable_index.modify(votetable_obj, voter, [&]( auto &item ) {
               item.voter = voter;
               item.percent = FIXED_CURATOR_PERCENT;
               item.weight = weight;
               item.time = now();
               item.rshares = rshares;
               item.count--;
            });
            break;
        }
        eosio_assert(posttable_obj != _post_table.end(), "This post doesn't exist.");
    }

    eosio::print("\ndownvote end");
}

void publication::close_post() {
    eosio::print("\nclose post");

    close_post_timer();
}

void publication::close_post_timer() {
    require_auth(_self);

    transaction trx;
    trx.actions.emplace_back(action{permission_level(_self, N(active)), _self, N(closepost), structures::st_hash{now()}});
    trx.delay_sec = CLOSE_POST_TIMER;
    trx.send(_self, _self);

    eosio::print("\nclose_post_timer");
}

