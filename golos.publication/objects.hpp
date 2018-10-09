#pragma once

#include <eosiolib/time.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/singleton.hpp>
#include <eosiolib/crypto.h>

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

struct closepost {
    closepost() = default;

    account_name account;
    std::string permlink;

    EOSLIB_SERIALIZE(closepost, (account)(permlink))
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

struct common_post_data {
    common_post_data() = default;

    account_name account;
    account_name parentacc;
    uint64_t curatorprcnt;
    std::string payouttype;
    std::vector<structures::beneficiary> beneficiaries;
    std::string paytype;

    EOSLIB_SERIALIZE(common_post_data, (account)(parentacc)(curatorprcnt)
                     (payouttype)(beneficiaries)(paytype))
};

struct createpost : common_post_data {
    createpost() = default;

    std::string permlink;
    std::string parentprmlnk;

    EOSLIB_SERIALIZE(createpost, (account)(permlink)(parentacc)(parentprmlnk)(curatorprcnt)
                     (payouttype)(beneficiaries)(paytype))
};

struct post : common_post_data {
    post() = default;

    uint64_t id;
    uint64_t date;
    checksum256 permlink;
    checksum256 parentprmlnk;
    uint64_t childcount;
    uint8_t isclosed;

    uint64_t primary_key() const {
        return id;
    }

    key256 permlink_hash_key() const {
        return get_hash_key(permlink);
    }

    key256 parentprmlnk_hash_key() const {
        return get_hash_key(parentprmlnk);
    }

    static key256 get_hash_key(const checksum256& permlink) {
        const uint64_t *p64 = reinterpret_cast<const uint64_t *>(&permlink);
        return key256::make_from_word_sequence<uint64_t>(p64[0], p64[1], p64[2], p64[3]);
    }

    EOSLIB_SERIALIZE(post, (id)(date)(account)(permlink)(parentacc)(parentprmlnk)(curatorprcnt)(payouttype)
                     (beneficiaries)(paytype)(childcount)(isclosed))
};

struct voteinfo {
    voteinfo() = default;

    uint64_t id;
    uint64_t post_id;
    account_name voter;
    uint64_t percent;
    int64_t weight;
    uint64_t time;
    std::vector<structures::rshares> rshares;
    uint64_t count;

    uint64_t primary_key() const {
        return id;
    }

    uint64_t secondary_key() const {
        return post_id;
    }

    EOSLIB_SERIALIZE(voteinfo, (id)(post_id)(voter)(percent)(weight)(time)(rshares)(count))
};

}

namespace tables {
    using id_index = indexed_by<N(id), const_mem_fun<structures::post, uint64_t, &structures::post::primary_key>>;
    using permlink_hash_index = indexed_by<N(permlink), const_mem_fun<structures::post, key256, &structures::post::permlink_hash_key>>;
    using parentprmlnk_hash_index = indexed_by<N(parentprmlnk), const_mem_fun<structures::post, key256, &structures::post::parentprmlnk_hash_key>>;
    using post_table = eosio::multi_index<N(posttable), structures::post, id_index, permlink_hash_index, parentprmlnk_hash_index>;

    using content_id_index = indexed_by<N(id), const_mem_fun<structures::content, uint64_t, &structures::content::primary_key>>;
    using content_table = eosio::multi_index<N(contenttable), structures::content, content_id_index>;

    using vote_id_index = indexed_by<N(id), const_mem_fun<structures::voteinfo, uint64_t, &structures::voteinfo::primary_key>>;
    using vote_postid_index = indexed_by<N(postid), const_mem_fun<structures::voteinfo, uint64_t, &structures::voteinfo::secondary_key>>;
    using vote_table = eosio::multi_index<N(votetable), structures::voteinfo, vote_id_index, vote_postid_index>;

    using accounts_battery_table = eosio::singleton<N(batterytable), structures::account_battery>;
}
