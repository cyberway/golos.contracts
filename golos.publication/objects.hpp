#pragma once

#include <eosiolib/time.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/singleton.hpp>
#include <eosiolib/crypto.h>

using namespace eosio;

namespace structures {

struct accandvalue {
    account_name account;
    uint64_t value;
};

using beneficiary = accandvalue;

struct tag {
    tag() = default;

    std::string tag_name;

    EOSLIB_SERIALIZE(tag, (tag_name))
};

struct content {
    content() = default;

    uint64_t id;
    //std::string permlink; //?
    std::string headermssg;
    std::string bodymssg;
    std::string languagemssg;
    std::vector<structures::tag> tags;
    std::string jsonmetadata;

    uint64_t primary_key() const {
        return id;
    }
};

struct message {
    message() = default;

    uint64_t id;
    uint64_t date;
    account_name parentacc;
    uint64_t parent_id;
    std::vector<structures::beneficiary> beneficiaries;
    uint64_t childcount;
    bool closed;

    uint64_t primary_key() const {
        return id;
    }
};

struct voteinfo {
    voteinfo() = default;

    uint64_t id;
    uint64_t message_id;
    account_name voter;
    int16_t weight;
    uint64_t time;
    int64_t count;

    uint64_t primary_key() const {
        return id;
    }

    uint64_t secondary_key() const {
        return message_id;
    }
};

}

namespace tables {
    using id_index = indexed_by<N(id), const_mem_fun<structures::message, uint64_t, &structures::message::primary_key>>;
    using message_table = eosio::multi_index<N(messagetable), structures::message, id_index>;

    using content_id_index = indexed_by<N(id), const_mem_fun<structures::content, uint64_t, &structures::content::primary_key>>;
    using content_table = eosio::multi_index<N(contenttable), structures::content, content_id_index>;

    using vote_id_index = indexed_by<N(id), const_mem_fun<structures::voteinfo, uint64_t, &structures::voteinfo::primary_key>>;
    using vote_messageid_index = indexed_by<N(messageid), const_mem_fun<structures::voteinfo, uint64_t, &structures::voteinfo::secondary_key>>;
    using vote_table = eosio::multi_index<N(votetable), structures::voteinfo, vote_id_index, vote_messageid_index>;

}
