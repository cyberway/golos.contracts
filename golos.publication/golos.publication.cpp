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

    if (N(createacc) == action)
        execute_action(this, &publication::create_battery_user);
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

    if (!parentacc) {
        recovery_battery(account, &structures::account_battery::posting_battery);
        limit_battery_account(account, uint64_t(POST_OPERATION_INTERVAL), &structures::account_battery::limit_battery_posting);
    } else {
        limit_battery_account(account, uint64_t(COMMENT_OPERATION_INTERVAL), &structures::account_battery::limit_battery_comment);
    }

    tables::post_table post_table(_self, account);
    tables::content_table content_table(_self, account);

    auto posttable_index = post_table.get_index<N(account)>();
    auto posttable_obj = posttable_index.find(account);

    while (posttable_obj != posttable_index.end() && posttable_obj->account == account) {
        eosio_assert(posttable_obj->permlink != permlink, "This post already exists.");
        ++posttable_obj;
    }

    auto post_id = post_table.available_primary_key();

    post_table.emplace(account, [&]( auto &item ) {
        item.id = post_id;
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

    content_table.emplace(account, [&]( auto &item ) {
        item.id = post_id;
        item.headerpost = headerpost;
        item.bodypost = bodypost;
        item.languagepost = languagepost;
        item.tags = tags;
        item.jsonmetadata = jsonmetadata;
    });
}

void publication::update_post(account_name account, std::string permlink,
                              std::string headerpost, std::string bodypost,
                              std::string languagepost, std::vector<structures::tag> tags,
                              std::string jsonmetadata) {
    require_auth(account);

    tables::content_table content_table(_self, account);
    structures::post posttable_obj;

    if (get_post(account, permlink, posttable_obj)) {
        auto contenttable_index = content_table.get_index<N(id)>();
        auto contenttable_obj = contenttable_index.find(posttable_obj.id);

        contenttable_index.modify(contenttable_obj, account, [&]( auto &item ) {
            item.headerpost = headerpost;
            item.bodypost = bodypost;
            item.languagepost = languagepost;
            item.tags = tags;
            item.jsonmetadata = jsonmetadata;
        });
    }
}

void publication::delete_post(account_name account, std::string permlink) {
    require_auth(account);

    tables::post_table post_table(_self, account);
    tables::content_table content_table(_self, account);
    tables::vote_table vote_table(_self, account);
    tables::voters_table voters_table(_self, account);

    structures::post posttable_obj;

    if (get_post(account, permlink, posttable_obj)) {
        if (posttable_obj.parentacc) {
            auto parentacc_index = post_table.get_index<N(parentacc)>();
            auto parentacc_obj = parentacc_index.find(account);
            while (parentacc_obj != parentacc_index.end() && parentacc_obj->parentacc == account) {
                eosio_assert(posttable_obj.parentprmlnk != parentacc_obj->parentprmlnk,
                             "You can't delete comment with child comments.");
                ++parentacc_obj;
            }
            auto posttable_index = post_table.get_index<N(id)>();
            auto post = posttable_index.find(posttable_obj.id);
            posttable_index.erase(post);
            auto contenttable_index = content_table.get_index<N(id)>();
            auto contenttable_obj = contenttable_index.find(posttable_obj.id);
            contenttable_index.erase(contenttable_obj);
            auto votetable_index = vote_table.get_index<N(postid)>();
            auto votetable_obj = votetable_index.find(posttable_obj.id);
            votetable_index.erase(votetable_obj);
            auto voterstable_index = voters_table.get_index<N(postid)>();
            auto voterstable_obj = voterstable_index.find(posttable_obj.id);
            while (voterstable_obj != voterstable_index.end())
                voterstable_obj = voterstable_index.erase(voterstable_obj);
        } else {
            eosio_assert(false, "You can't delete post, it can be only changed.");
        }
    }
}

void publication::upvote(account_name voter, account_name author, std::string permlink, asset weight) {
    require_auth(voter);
    limit_battery_account(voter, uint64_t(VOTE_OPERATION_INTERVAL), &structures::account_battery::limit_battery_comment);

    tables::vote_table vote_table(_self, author);
    tables::voters_table voters_table(_self, author);

    std::vector<structures::rshares> rshares;

    structures::rshares rshares_obj{0, 0, 0, 0};

    rshares.push_back(rshares_obj);

    structures::post posttable_obj;

    if (get_post(author, permlink, posttable_obj)) {
        auto votetable_index = vote_table.get_index<N(postid)>();
        auto votetable_obj = votetable_index.find(posttable_obj.id);
        if (votetable_obj != votetable_index.end()) {
            auto voterstable_index = voters_table.get_index<N(postid)>();
            auto voterstable_obj = voterstable_index.find(posttable_obj.id);
            while (voterstable_obj != voterstable_index.end() &&
                   posttable_obj.id == voterstable_obj->post_id) {
                eosio_assert(voter != voterstable_obj->voter,
                             "You have already voted for this post.");
                ++voterstable_obj;
            }

            voters_table.emplace(author, [&]( auto &item ) {
                item.id = voters_table.available_primary_key();
                item.post_id = posttable_obj.id;
                item.voter = voter;
            });

            votetable_index.modify(votetable_obj, author, [&]( auto &item ) {
               item.post_id = posttable_obj.id;
               item.voter = voter;
               item.percent = FIXED_CURATOR_PERCENT;
               item.weight = weight;
               item.time = now();
               item.rshares = rshares;
               ++item.count;
            });
        } else {
            voters_table.emplace(author, [&]( auto &item ) {
                item.id = voters_table.available_primary_key();
                item.post_id = posttable_obj.id;
                item.voter = voter;
            });

            vote_table.emplace(author, [&]( auto &item ) {
               item.post_id = posttable_obj.id;
               item.voter = voter;
               item.percent = FIXED_CURATOR_PERCENT;
               item.weight = weight;
               item.time = now();
               item.rshares = rshares;
               ++item.count;
            });
        }
    }
}

void publication::downvote(account_name voter, account_name author, std::string permlink, asset weight) {
    require_auth(voter);

    tables::vote_table vote_table(_self, author);
    tables::voters_table voters_table(_self, author);

    std::vector<structures::rshares> rshares;

    structures::rshares rshares_obj{0, 0, 0, 0};

    rshares.push_back(rshares_obj);

    structures::post posttable_obj;

    if (get_post(author, permlink, posttable_obj)) {
        auto voterstable_index = voters_table.get_index<N(postid)>();
        auto voterstable_obj = voterstable_index.find(posttable_obj.id);
        while (voterstable_obj != voterstable_index.end() && posttable_obj.id == voterstable_obj->post_id) {
            if (voter == voterstable_obj->voter) {
                voterstable_index.erase(voterstable_obj);
                break;
            } else {
                ++voterstable_obj;
            }
        }

        eosio_assert(voterstable_obj != voterstable_index.end(),
                     "You can't do down vote because previously you haven't voted for this post.");

        auto votetable_index = vote_table.get_index<N(postid)>();
        auto votetable_obj = votetable_index.find(posttable_obj.id);

        votetable_index.modify(votetable_obj, author, [&]( auto &item ) {
           item.voter = voter;
           item.percent = FIXED_CURATOR_PERCENT;
           item.weight = weight;
           item.time = now();
           item.rshares = rshares;
           --item.count;
        });
    }
}

void publication::close_post() {
    close_post_timer();
}

void publication::close_post_timer() {
    require_auth(_self);

    transaction trx;
    trx.actions.emplace_back(action{permission_level(_self, N(active)), _self, N(closepost), structures::st_hash{now()}});
    trx.delay_sec = CLOSE_POST_TIMER;
    trx.send(_self, _self);
}

bool publication::get_post(account_name account, std::string permlink, structures::post &post) {
    tables::post_table post_table(_self, account);

    auto posttable_index = post_table.get_index<N(account)>();
    auto posttable_obj = posttable_index.find(account);

    while (posttable_obj != posttable_index.end() && posttable_obj->account == account) {
        if (posttable_obj->permlink == permlink) {
            post = *posttable_obj;
            return true;
        } else {
            ++posttable_obj;
        }
    }
    eosio_assert(false, "Post doesn't exist.");
    return false;
}

void publication::create_battery_user(account_name name) {
    require_auth(name);
    tables::accounts_battery_table account_battery(_self, name);
    eosio_assert(!account_battery.exists(), "This user already exists");

    structures::battery st_battery{UPPER_BOUND, time_point_sec(now())};
    account_battery.set(structures::account_battery{st_battery, st_battery, st_battery, st_battery, st_battery},
                        name);
}

template<typename T>
void publication::limit_battery_account(scope_name scope, uint64_t interval,
                                            T structures::account_battery::*element) {
    tables::accounts_battery_table account_battery(_self, scope);
    auto obj = account_battery.get();
    auto battery = obj.*element;

    eosio_assert(battery.renewal <= time_point_sec(now()), "Action not allowed by timeout");

    battery.renewal = time_point_sec(now() + interval);
    obj.*element = battery;

    account_battery.set(obj, scope);
}

template<typename T>
void publication::recovery_battery(scope_name scope, T structures::account_battery::*element, uint16_t persent_battery) {
    tables::accounts_battery_table table(_self, scope);
    eosio_assert(table.exists(), "Not found battery of this user");

    const auto current_time = now();
    auto account_battery = table.get();
    auto battery = account_battery.*element;
    auto recovery_sec = current_time - battery.renewal.utc_seconds;

    double recovery_change;
    auto lambda = [&]{
        uint64_t battery_charge = recovery_change + battery.charge;
        if (battery_charge > UPPER_BOUND) {
            battery.charge = UPPER_BOUND;
            battery.renewal = time_point_sec(current_time);
        } else {
            battery.charge = battery_charge;
            battery.renewal = time_point_sec(current_time);
        }
    };

    if(!persent_battery) {
        recovery_change = (UPPER_BOUND - battery.charge) * recovery_sec / RECOVERY_PERIOD_POSTING;

        lambda();

        eosio_assert(battery.charge >= POST_AMOUNT_OPERATIONS, "Not enough battery power to create a post");
        battery.charge -= POST_AMOUNT_OPERATIONS;

    } else {
        eosio_assert(persent_battery <= UPPER_BOUND && persent_battery >= LOWER_BOUND, "Invalid persent battary");
        recovery_change = (UPPER_BOUND * recovery_sec) / RECOVERY_PERIOD_VESTING;

        lambda();

        eosio_assert(battery.charge >= persent_battery, "Not enough charge");
        battery.charge -= persent_battery;
    }

    account_battery.*element = battery;
    table.set(account_battery, scope);
}

