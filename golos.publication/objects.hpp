#pragma once

#include <eosiolib/time.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/singleton.hpp>
#include <eosiolib/crypto.h>

using namespace eosio;

namespace structures {
    
struct postkey {
    postkey() = default;

    account_name account;
    std::string permlink;
};

struct beneficiary {
    beneficiary() = default;

    account_name account;
    uint64_t deductprcnt;
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
};

struct post {
    post() = default;

    uint64_t id;
    uint64_t date;
    checksum256 permlink;
    account_name parentacc;
    checksum256 parentprmlnk;
    std::vector<structures::beneficiary> beneficiaries;
    uint64_t childcount;
    bool closed;

    uint64_t primary_key() const {
        return id;
    }

    key256 permlink_hash_key() const {
        return get_hash_key(permlink);
    }

    static key256 get_hash_key(const checksum256& permlink) {
        const uint64_t *p64 = reinterpret_cast<const uint64_t *>(&permlink);
        return key256::make_from_word_sequence<uint64_t>(p64[0], p64[1], p64[2], p64[3]);
    }
};

struct voteinfo {
    voteinfo() = default;

    uint64_t id;
    uint64_t post_id;
    account_name voter;
    int16_t weight;
    uint64_t time;
    int64_t count;

    uint64_t primary_key() const {
        return id;
    }

    uint64_t secondary_key() const {
        return post_id;
    }
};

}

namespace tables {
    using id_index = indexed_by<N(id), const_mem_fun<structures::post, uint64_t, &structures::post::primary_key>>;
    using permlink_hash_index = indexed_by<N(permlink), const_mem_fun<structures::post, key256, &structures::post::permlink_hash_key>>;
    using post_table = eosio::multi_index<N(posttable), structures::post, id_index, permlink_hash_index>;

    using content_id_index = indexed_by<N(id), const_mem_fun<structures::content, uint64_t, &structures::content::primary_key>>;
    using content_table = eosio::multi_index<N(contenttable), structures::content, content_id_index>;

    using vote_id_index = indexed_by<N(id), const_mem_fun<structures::voteinfo, uint64_t, &structures::voteinfo::primary_key>>;
    using vote_postid_index = indexed_by<N(postid), const_mem_fun<structures::voteinfo, uint64_t, &structures::voteinfo::secondary_key>>;
    using vote_table = eosio::multi_index<N(votetable), structures::voteinfo, vote_id_index, vote_postid_index>;

}
