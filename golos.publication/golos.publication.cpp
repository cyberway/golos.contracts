#include "golos.publication.hpp"
#include "config.hpp"

#include <eosiolib/transaction.hpp>

#define NULL_LAMBDA [](auto&, auto&) {}

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

    checksum256 permlink_hash = get_checksum256(permlink);
    checksum256 parentprmlnk_hash = get_checksum256(parentprmlnk);

    structures::post posttable_obj;
    if (get_post(account, permlink, posttable_obj, NULL_LAMBDA))
        post_assert(true);

    auto post_id = post_table.available_primary_key();

    post_table.emplace(account, [&]( auto &item ) {
        item.id = post_id;
        item.date = now();
        item.account = account;
        item.permlink = permlink_hash;
        item.parentacc = parentacc;
        item.parentprmlnk = parentprmlnk_hash;
        item.curatorprcnt = curatorprcnt;
        item.payouttype = payouttype;
        item.beneficiaries = beneficiaries;
        item.paytype = paytype;
        if (parentacc) {
            set_child_count(true, parentacc, parentprmlnk_hash);
        }
        item.status = post_status::open;
    });

    content_table.emplace(account, [&]( auto &item ) {
        item.id = post_id;
        item.headerpost = headerpost;
        item.bodypost = bodypost;
        item.languagepost = languagepost;
        item.tags = tags;
        item.jsonmetadata = jsonmetadata;
    });

    close_post_timer(account, permlink);
}

void publication::update_post(account_name account, std::string permlink,
                              std::string headerpost, std::string bodypost,
                              std::string languagepost, std::vector<structures::tag> tags,
                              std::string jsonmetadata) {
    require_auth(account);

    tables::content_table content_table(_self, account);
    structures::post posttable_obj;

    if (get_post(account, permlink, posttable_obj, NULL_LAMBDA)) {
        auto contenttable_obj = content_table.find(posttable_obj.id);

        content_table.modify(contenttable_obj, account, [&]( auto &item ) {
            item.headerpost = headerpost;
            item.bodypost = bodypost;
            item.languagepost = languagepost;
            item.tags = tags;
            item.jsonmetadata = jsonmetadata;
        });
    } else
        post_assert(false);
}

void publication::delete_post(account_name account, std::string permlink) {
    require_auth(account);

    tables::post_table post_table(_self, account);
    tables::content_table content_table(_self, account);
    tables::vote_table vote_table(_self, account);

    structures::post posttable_obj;

    if (get_post(account, permlink, posttable_obj, NULL_LAMBDA)) {
        eosio_assert(posttable_obj.childcount == 0,
                     "You can't delete comment with child comments.");
        set_child_count(false, posttable_obj.parentacc, posttable_obj.parentprmlnk);
        post_table.erase(post_table.find(posttable_obj.id));
        content_table.erase(content_table.find(posttable_obj.id));
        auto votetable_index = vote_table.get_index<N(postid)>();
        auto votetable_obj = votetable_index.find(posttable_obj.id);
        while (votetable_obj != votetable_index.end())
            votetable_obj = votetable_index.erase(votetable_obj);
    } else
        post_assert(false);
}

void publication::upvote(account_name voter, account_name author, std::string permlink, int16_t weight) {
    require_auth(voter);
    recovery_battery(voter, &structures::account_battery::limit_battery_of_votes, {UPPER_BOUND, VOTE_OPERATION_INTERVAL, UPPER_BOUND, type_recovery::linear});

    eosio_assert(weight > 0, "The weight must be positive.");
    eosio_assert(weight <= MAX_WEIGHT, "The weight can't be more than 100%.");

    set_vote(voter, author, permlink, weight);
}

void publication::downvote(account_name voter, account_name author, std::string permlink, int16_t weight) {
    require_auth(voter);

    eosio_assert(weight > 0, "The weight sign can't be negative.");
    eosio_assert(weight <= MAX_WEIGHT, "The weight can't be more than 100%.");

    set_vote(voter, author, permlink, -weight);
}

void publication::unvote(account_name voter, account_name author, std::string permlink) {
    require_auth(voter);
  
    set_vote(voter, author, permlink, 0);
}

void publication::close_post(account_name account, std::string permlink) {
    structures::post post_obj;

    get_post(account, permlink, post_obj, [&](auto &posttable_index, auto &posttable_obj) {
        posttable_index.modify(posttable_obj, account, [&]( auto &item) {
            item.status = post_status::closed;
        });
    });
}

void publication::close_post_timer(account_name account, std::string permlink) {
    require_auth(account);

    transaction trx;
    trx.actions.emplace_back(action{permission_level(_self, N(active)), _self, N(closepost), structures::closepost{account, permlink}});
    trx.delay_sec = CLOSE_POST_PERIOD;
    trx.send(_self, _self);
}

// weight is fixed point with two decimal digits,
// if you need to operate it,
// please divide by TWO_DECIMAL_DIGITS
void publication::set_vote(account_name voter, account_name author,
                           std::string permlink, int16_t weight) {
    tables::vote_table vote_table(_self, author);

    structures::post posttable_obj;

    std::vector<structures::rshares> rshares{{0, 0, 0, 0}};

    if (get_post(author, permlink, posttable_obj, NULL_LAMBDA)) {
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
                   if (posttable_obj.status == post_status::open)
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
           if (posttable_obj.status == post_status::open)
               item.rshares = rshares;
           ++item.count;
        });
    } else
        post_assert(false);
}

template<typename Lambda>
bool publication::get_post(account_name account, checksum256 &permlink, structures::post &post, Lambda &&lambda) {
    tables::post_table post_table(_self, account);

    auto posttable_index = post_table.get_index<N(permlink)>();
    auto posttable_obj = posttable_index.find(structures::post::get_hash_key(permlink));

    if (posttable_obj != posttable_index.end()) {
        lambda(posttable_index, posttable_obj);
        post = *posttable_obj;
        return true;
    } else
        return false;
}

template<typename Lambda>
bool publication::get_post(account_name account, std::string &permlink, structures::post &post, Lambda &&lambda) {
    checksum256 checksum = get_checksum256(permlink);
    return get_post(account, checksum, post, lambda);
}

checksum256 publication::get_checksum256(std::string &permlink) {
    checksum256 hash;
    sha256(permlink.c_str(), sizeof(permlink), &hash);
    return hash;
}

void publication::set_child_count(bool increase, account_name parentacc, checksum256 &parentprmlnk) {
    structures::post post_obj;
    get_post(parentacc, parentprmlnk, post_obj, [&](auto &posttable_index, auto &posttable_obj) {
        posttable_index.modify(posttable_obj, 0, [&]( auto &item ) {
            if (increase)
                ++item.childcount;
            else
                --item.childcount;
        });
    });
}

void publication::post_assert(bool exist) {
    if (exist)
        eosio_assert(false, "This post already exists.");
    else
        eosio_assert(false, "Post doesn't exist.");
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

    if (element == &structures::account_battery::limit_battery_posting)
        consume_allow_overusage(battery, params);
    else
        consume(battery, params);

    account_battery.*element = battery;
    table.set(account_battery, scope);
}
