#pragma once
#include <eosiolib/eosio.hpp>
#include "types.h"
#include "config.hpp"
#include <common/calclib/atmsp_storable.h>
#include <eosiolib/time.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/singleton.hpp>
#include <eosiolib/crypto.h>

namespace golos { namespace structures {

using namespace eosio;

struct accandvalue {
    name account;
    uint64_t value;
};
using counter_t = uint64_t;
struct beneficiary {
    name account;
    int64_t deductprcnt; //elaf_t
};

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

struct messagestate {
    base_t netshares = 0;
    base_t voteshares = 0;
    base_t sumcuratorsw = 0;
};

struct message {
    message() = default;

    uint64_t id;
    std::string permlink;
    uint64_t date;
    name parentacc;
    uint64_t parent_id;
    base_t tokenprop; //elaf_t
    std::vector<structures::beneficiary> beneficiaries;
    base_t rewardweight;//elaf_t
    messagestate state;
    uint64_t childcount;
    bool closed;
    uint16_t level;

    uint64_t primary_key() const {
        return id;
    }
};

struct voteinfo {
    voteinfo() = default;

    uint64_t id;
    uint64_t message_id;
    name voter;
    int16_t weight;
    uint64_t time;
    int64_t count;

    base_t curatorsw;
    base_t rshares;

    uint64_t primary_key() const {
        return id;
    }

    uint64_t by_message() const {
        return message_id;
    }
};

struct funcinfo {
    atmsp::storable::bytecode code;
    base_t maxarg;
};

struct rewardrules {
    funcinfo mainfunc;
    funcinfo curationfunc;
    funcinfo timepenalty;
    base_t curatorsprop; //elaf_t
    base_t maxtokenprop; //elaf_t
    //uint64_t cashout_time; //TODO:
};

struct forumprops_record {
    forumprops_record() = default;

    forumprops_record(const forumprops& props) {
        social_contract = props.social_contract;
    }

    name social_contract = name();

    EOSLIB_SERIALIZE(forumprops_record, (social_contract))
};

struct poolstate {
    counter_t msgs;
    eosio::asset funds;
    wide_t rshares;
    wide_t rsharesfn;

    using ratio_t = decltype(elap_t(1) / elap_t(1));
    ratio_t get_ratio() const {
        eosio_assert(funds.amount >= 0, "poolstate::get_ratio: funds < 0");
        auto r = WP(rshares);
        auto f = fixp_t(funds.amount);
        narrow_down(f, r);
        return r > 0 ? elap_t(f) / elap_t(r) : std::numeric_limits<ratio_t>::max();
    }
};

struct rewardpool {
    uint64_t created;
    rewardrules rules;
    poolstate state;

    uint64_t primary_key() const { return created; }
    EOSLIB_SERIALIZE(rewardpool, (created)(rules)(state))
};

struct limitparams {
    enum act_t: uint8_t {POST, COMM, VOTE, POSTBW};
    uint64_t act;
    uint8_t charge_id;
    int64_t price;
    int64_t cutoff;
    int64_t vesting_price;
    int64_t min_vesting;
    uint64_t primary_key() const { return act; }
    static inline act_t act_from_str(const std::string& arg) {
        if(arg == "post") return POST;
        if(arg == "comment") return COMM;
        if(arg == "vote") return VOTE;
        eosio_assert(arg == "post bandwidth", "limitparams::act_from_str: wrong arg");
        return POSTBW;
    }
};

} // structures

namespace tables {

using namespace eosio;

using id_index = indexed_by<N(id), const_mem_fun<structures::message, uint64_t, &structures::message::primary_key>>;
using message_table = multi_index<N(message), structures::message, id_index>;

using content_id_index = indexed_by<N(id), const_mem_fun<structures::content, uint64_t, &structures::content::primary_key>>;
using content_table = multi_index<N(content), structures::content, content_id_index>;

using vote_id_index = indexed_by<N(id), const_mem_fun<structures::voteinfo, uint64_t, &structures::voteinfo::primary_key>>;
using vote_messageid_index = indexed_by<N(messageid), const_mem_fun<structures::voteinfo, uint64_t, &structures::voteinfo::by_message>>;
using vote_table = multi_index<N(vote), structures::voteinfo, vote_id_index, vote_messageid_index>;

using reward_pools = multi_index<N(rewardpools), structures::rewardpool>;
using limit_table = multi_index<N(limit), structures::limitparams>;

using forumprops_singleton = eosio::singleton<"forumprops"_n, structures::forumprops_record>;

}


} // golos
