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
        recovery_battery(account, &structures::account_battery::limit_battery_posting, {UPPER_BOUND, POST_OPERATION_INTERVAL, UPPER_BOUND, type_recovery::linear});
        recovery_battery(account, &structures::account_battery::posting_battery, {UPPER_BOUND, RECOVERY_PERIOD_POSTING, POST_AMOUNT_OPERATIONS, type_recovery::persent}); // TODO battery that is overrun
    } else {
        recovery_battery(account, &structures::account_battery::limit_battery_comment, {UPPER_BOUND, COMMENT_OPERATION_INTERVAL, UPPER_BOUND, type_recovery::linear});
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
    recovery_battery(voter, &structures::account_battery::limit_battery_of_votes, {UPPER_BOUND, VOTE_OPERATION_INTERVAL, UPPER_BOUND, type_recovery::linear});

//    eosio_assert(weight > 0, "The weight sign can't be negative.");

    set_vote(voter, author, permlink, weight);
}

void publication::downvote(account_name voter, account_name author, std::string permlink, asset weight) {
    require_auth(voter);

//    eosio_assert(weight < 0, "The weight sign can't be positive.");

    set_vote(voter, author, permlink, weight);
}

void publication::unvote(account_name voter, account_name author, std::string permlink) {
    require_auth(voter);

//    eosio_assert(weight == 0, "The weight can be only zero.");

    set_vote(voter, author, permlink, asset(0, S(4, GOLOS)));
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
                           std::string permlink, asset weight) {
    tables::vote_table vote_table(_self, author);

    structures::post posttable_obj;

    std::vector<structures::rshares> rshares;

    structures::rshares rshares_obj{0, 0, 0, 0};

    rshares.push_back(rshares_obj);

    if (get_post(author, permlink, posttable_obj)) {
        auto votetable_index = vote_table.get_index<N(postid)>();
        auto votetable_obj = votetable_index.find(posttable_obj.id);

        while (votetable_obj != votetable_index.end()) {
            if (voter == votetable_obj->voter) {
                eosio_assert(weight != votetable_obj->weight, "Vote with the same weight has already existed.");
                eosio_assert(votetable_obj->count != MAX_REVOTES, "You can't revote anymore.");
                votetable_index.modify(votetable_obj, voter, [&]( auto &item ) {
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
        vote_table.emplace(voter, [&]( auto &item ) {
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

    structures::battery st_battery{LOWER_BOUND, time_point_sec(now())};
    account_battery.set(structures::account_battery{st_battery, st_battery, st_battery, st_battery, st_battery}, name);
}

int64_t publication::current_consumed(const structures::battery &battery, const structures::params_battery &params) {
    auto sec = now() - battery.renewal.utc_seconds;
    int64_t recovery = sec * (params.mode == type_recovery::linear ? params.max_charge : battery.charge) / params.time_to_charge;

    auto consumed = battery.charge - recovery;
    if (consumed > 0)
        return consumed;

    return 0;
}

int64_t publication::consume(structures::battery &battery, const structures::params_battery &params) {
    auto consumed = params.consume_battery + current_consumed(battery, params);

    eosio_assert( (consumed <= params.max_charge), "Battery overrun" );

    battery.charge = consumed;
    battery.renewal = time_point_sec(now());

    return consumed;
}

int64_t publication::consume_allow_overusage(structures::battery &battery, const structures::params_battery &params) {
    auto consumed = params.consume_battery + current_consumed(battery, params);

    battery.charge = consumed;
    battery.renewal = time_point_sec(now());

    return consumed;
}

template<typename T>
void publication::recovery_battery(scope_name scope, T structures::account_battery::*element, const structures::params_battery &params) {
    tables::accounts_battery_table table(_self, scope);
    eosio_assert(table.exists(), "Not found battery of this user");

    auto account_battery = table.get();
    auto battery = account_battery.*element;

    if (element == &structures::account_battery::posting_battery)
        consume_allow_overusage(battery, params);
    else
        consume(battery, params);

    account_battery.*element = battery;
    table.set(account_battery, scope);
}
