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
        limit_posts(account);
        posting_battery(account);
    } else {
        limit_comments(account);
    }

    tables::post_table post_table(_self, account);
    tables::content_table content_table(_self, account);

    checksum256 permlink_hash;
    sha256(permlink.c_str(), sizeof(permlink), &permlink_hash);

    auto posttable_index = post_table.get_index<N(permlink)>();
    auto posttable_obj = posttable_index.find(structures::post::get_hash_key(permlink_hash));

    eosio_assert(posttable_obj == posttable_index.end(), "This post already exists.");

    auto post_id = post_table.available_primary_key();

    post_table.emplace(account, [&]( auto &item ) {
        item.id = post_id;
        item.date = now();
        item.account = account;
        item.permlink = permlink_hash;
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
            while (votetable_obj != votetable_index.end())
                votetable_obj = votetable_index.erase(votetable_obj);
        } else {
            eosio_assert(false, "You can't delete post, it can be only changed.");
        }
    }
}

void publication::upvote(account_name voter, account_name author, std::string permlink, asset weight) {
    require_auth(voter);
    limit_of_votes(voter);

//    eosio_assert(weight > 0, "The weight sign can't be negative.");

    std::vector<structures::rshares> rshares;

    structures::rshares rshares_obj{0, 0, 0, 0};

    rshares.push_back(rshares_obj);

    set_vote(voter, author, permlink, weight, rshares);
}

void publication::downvote(account_name voter, account_name author, std::string permlink, asset weight) {
    require_auth(voter);

//    eosio_assert(weight < 0, "The weight sign can't be positive.");

    std::vector<structures::rshares> rshares;

    structures::rshares rshares_obj{0, 0, 0, 0};

    rshares.push_back(rshares_obj);

    set_vote(voter, author, permlink, weight, rshares);
}

void publication::unvote(account_name voter, asset weight) {
    require_auth(voter);
//    eosio_assert(weight == 0, "The weight can be only zero.");
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

void publication::set_vote(account_name voter, account_name author,
                           std::string permlink, asset weight,
                           std::vector<structures::rshares> rshares) {
    tables::vote_table vote_table(_self, author);

    structures::post posttable_obj;

    if (get_post(author, permlink, posttable_obj)) {
        auto votetable_index = vote_table.get_index<N(postid)>();
        auto votetable_obj = votetable_index.find(posttable_obj.id);

        while (votetable_obj != votetable_index.end()) {
            if (voter == votetable_obj->voter) {
                eosio_assert(weight != votetable_obj->weight, "Vote with the same weight has already existed.");
                eosio_assert(votetable_obj->count != MAX_REVOTES, "You can't revote anymore.");
                votetable_index.modify(votetable_obj, author, [&]( auto &item ) {
                   item.post_id = posttable_obj.id;
                   item.voter = voter;
                   item.percent = FIXED_CURATOR_PERCENT;
                   item.weight = weight;
                   item.time = now();
                   item.rshares = rshares;
                   ++item.count;
                });
                return;
            }
            ++votetable_obj;
        }
        vote_table.emplace(author, [&]( auto &item ) {
           item.id = vote_table.available_primary_key();
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

bool publication::get_post(account_name account, std::string permlink, structures::post &post) {
    tables::post_table post_table(_self, account);

    checksum256 permlink_hash;
    sha256(permlink.c_str(), sizeof(permlink), &permlink_hash);
    auto posttable_index = post_table.get_index<N(permlink)>();
    auto posttable_obj = posttable_index.find(structures::post::get_hash_key(permlink_hash));

    eosio_assert(posttable_obj != posttable_index.end(), "Post doesn't exist.");

    post = *posttable_obj;
    return true;
}

void publication::create_battery_user(account_name name) {
    require_auth(name);
    tables::accounts_battery_table account_battery(_self, name);
    eosio_assert(!account_battery.exists(), "This user already exists");

    structures::battery st_battery{UPPER_BOUND, time_point_sec(now())};
    account_battery.set(structures::account_battery{st_battery, st_battery, st_battery, st_battery, st_battery},
                        name);
}

void publication::limit_posts(scope_name scope) {
    tables::accounts_battery_table account_battery(_self, scope);
    eosio_assert(account_battery.exists(), "Not found battery of this user");

    auto battery = account_battery.get();
    eosio_assert(battery.limit_battery_posting.renewal <= time_point_sec(now()), "Сreating a post is prohibited");

    battery.limit_battery_posting.renewal = time_point_sec(now() + POST_OPERATION_INTERVAL);
    account_battery.set(battery, scope);
}

void publication::limit_comments(scope_name scope) {
    tables::accounts_battery_table account_battery(_self, scope);
    eosio_assert(account_battery.exists(), "Not found battery of this user");

    auto battery = account_battery.get();
    eosio_assert(battery.limit_battery_comment.renewal <= time_point_sec(now()), "Сreating a comment is prohibited");

    battery.limit_battery_comment.renewal = time_point_sec(now() + COMMENT_OPERATION_INTERVAL);
    account_battery.set(battery, scope);
}

void publication::limit_of_votes(scope_name scope) {
    tables::accounts_battery_table account_battery(_self, scope);
    eosio_assert(account_battery.exists(), "Not found battery of this user");

    auto battery = account_battery.get();
    eosio_assert(battery.limit_battery_of_votes.renewal <= time_point_sec(now()), "Сreating a vote is prohibited");

    battery.limit_battery_of_votes.renewal = time_point_sec(now() + VOTE_OPERATION_INTERVAL);
    account_battery.set(battery, scope);
}

void publication::posting_battery(scope_name scope) {
    tables::accounts_battery_table account_battery(_self, scope);
    eosio_assert(account_battery.exists(), "Not found battery of this user");

    const auto current_time = now();
    auto battery = account_battery.get();
    auto recovery_sec = current_time - battery.posting_battery.renewal.utc_seconds;

    double recovery_change = (UPPER_BOUND - battery.posting_battery.charge) * recovery_sec / RECOVERY_PERIOD_POSTING;
    uint64_t battery_charge = recovery_change + battery.posting_battery.charge;
    if (battery_charge > UPPER_BOUND) { // TODO recovery battery
        battery.posting_battery.charge = UPPER_BOUND;
        battery.posting_battery.renewal = time_point_sec(current_time);
    } else {
        battery.posting_battery.charge = battery_charge;
        battery.posting_battery.renewal = time_point_sec(current_time);
    }

    eosio_assert(battery.posting_battery.charge >= POST_AMOUNT_OPERATIONS, "Not enough battery power to create a post");
    battery.posting_battery.charge -= POST_AMOUNT_OPERATIONS;
    battery.posting_battery.renewal = time_point_sec(now());

    account_battery.set(battery, scope);
}

void publication::vesting_battery(scope_name scope, uint16_t persent_battery) {
    tables::accounts_battery_table account_battery(_self, scope);
    eosio_assert(account_battery.exists(), "Not found battery of this user");
    eosio_assert(persent_battery <= UPPER_BOUND && persent_battery >= LOWER_BOUND, "Invalid persent battary");

    const auto current_time = now();
    auto battery = account_battery.get();
    auto recovery_sec = current_time - battery.battery_of_votes.renewal.utc_seconds;

    double recovery_change = (UPPER_BOUND * recovery_sec) / RECOVERY_PERIOD_VESTING;
    uint64_t battery_charge = recovery_change + battery.battery_of_votes.charge;
    if (battery_charge > UPPER_BOUND) {
        battery.battery_of_votes.charge = UPPER_BOUND;
        battery.battery_of_votes.renewal = time_point_sec(current_time);
    } else {
        battery.battery_of_votes.charge = battery_charge;
        battery.battery_of_votes.renewal = time_point_sec(current_time);
    }

    eosio_assert(battery.battery_of_votes.charge >= persent_battery, "Not enough charge");
    battery.battery_of_votes.charge -= persent_battery;

    account_battery.set(battery, scope);
}
