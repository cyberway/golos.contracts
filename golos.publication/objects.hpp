#pragma once

#include <eosiolib/time.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/singleton.hpp>

using namespace eosio;

namespace structures {

struct battery {
    uint16_t charge;
    time_point_sec renewal;

    EOSLIB_SERIALIZE(battery, (charge)(renewal))
};

struct account_battery {
    battery posting_battery;
    battery battery_of_votes;

    battery limit_battery_posting;
    battery limit_battery_comment;
    battery limit_battery_of_votes;

    EOSLIB_SERIALIZE(account_battery, (posting_battery)(battery_of_votes)(limit_battery_posting)
                     (limit_battery_comment)(limit_battery_of_votes))
};

struct st_hash {
    st_hash() = default;

    uint64_t hash;

    EOSLIB_SERIALIZE(st_hash, (hash))
};

struct rshares {
    rshares() = default;

    uint64_t net_rshares;
    uint64_t abs_rshares;
    uint64_t vote_rshares;
    uint64_t children_abs_rshares;

    EOSLIB_SERIALIZE(rshares, (net_rshares)(abs_rshares)(vote_rshares)(children_abs_rshares))
};

struct beneficiary {
    beneficiary() = default;

    account_name account;
    uint64_t deductprcnt;

    EOSLIB_SERIALIZE(beneficiary, (account)(deductprcnt))
};

struct tag {
    tag() = default;

    std::string tag_name;

    EOSLIB_SERIALIZE(tag, (tag_name))
};

struct content {
    content() = default;

    uint64_t id;
    std::string headerpost;
    std::string bodypost;
    std::string languagepost;
    std::vector<structures::tag> tags;
    std::string jsonmetadata;

    uint64_t primary_key() const {
        return id;
    }

    EOSLIB_SERIALIZE(content, (id)(headerpost)(bodypost)(languagepost)
                     (tags)(jsonmetadata))
};

struct createpost {
    createpost() = default;

    account_name account;
    std::string permlink;
    account_name parentacc;
    std::string parentprmlnk;
    uint64_t curatorprcnt;
    std::string payouttype;
    std::vector<structures::beneficiary> beneficiaries;
    std::string paytype;

    EOSLIB_SERIALIZE(createpost, (account)(permlink)(parentacc)(parentprmlnk)(curatorprcnt)
                     (payouttype)(beneficiaries)(paytype))
};

struct post : createpost {
    post() = default;

    uint64_t id;
    uint64_t date;

    uint64_t primary_key() const {
        return id;
    }

    uint64_t account_key() const {
        return account;
    }

    uint64_t parentacc_key() const {
        return parentacc;
    }

    EOSLIB_SERIALIZE(post, (id)(date)(account)(permlink)(parentacc)(parentprmlnk)(curatorprcnt)(payouttype)
                     (beneficiaries)(paytype))
};

struct voteinfo {
    voteinfo() = default;

    uint64_t postid;
    account_name voter;
    uint64_t percent;
    asset weight;
    uint64_t time;
    std::vector<structures::rshares> rshares;
    uint64_t count;

    uint64_t primary_key() const {
        return postid;
    }

    EOSLIB_SERIALIZE(voteinfo, (postid)(voter)(percent)(weight)(time)(rshares)(count))
};

struct votersinfo {
    votersinfo() = default;

    uint64_t id;
    uint64_t postid;
    account_name voter;

    uint64_t primary_key() const {
        return id;
    }

    uint64_t postid_key() const {
        return postid;
    }

    EOSLIB_SERIALIZE(votersinfo, (id)(postid)(voter))
};

}

namespace tables {
    using id_index = indexed_by<N(id), const_mem_fun<structures::post, uint64_t, &structures::post::primary_key>>;
    using account_index = indexed_by<N(account), const_mem_fun<structures::post, uint64_t, &structures::post::account_key>>;
    using parentacc_index = indexed_by<N(parentacc), const_mem_fun<structures::post, uint64_t, &structures::post::parentacc_key>>;
    using post_table = eosio::multi_index<N(posttable), structures::post, id_index, account_index, parentacc_index>;

    using content_id_index = indexed_by<N(id), const_mem_fun<structures::content, uint64_t, &structures::content::primary_key>>;
    using content_table = eosio::multi_index<N(contenttable), structures::content, content_id_index>;

    using vote_index = indexed_by<N(postid), const_mem_fun<structures::voteinfo, uint64_t, &structures::voteinfo::primary_key>>;
    using vote_table = eosio::multi_index<N(votetable), structures::voteinfo, vote_index>;

    using voters_index = indexed_by<N(id), const_mem_fun<structures::votersinfo, uint64_t, &structures::votersinfo::primary_key>>;
    using postid_index = indexed_by<N(postid), const_mem_fun<structures::votersinfo, uint64_t, &structures::votersinfo::postid_key>>;
    using voters_table = eosio::multi_index<N(voterstable), structures::votersinfo, voters_index, postid_index>;

    using accounts_battery_table = eosio::singleton<N(batterytable), structures::account_battery>;
}
