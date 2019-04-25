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

using counter_t = uint64_t;
struct beneficiary {
    name account;
    int64_t deductprcnt; //elaf_t
};

struct mssgid {
    mssgid() = default;

    name author;
    std::string permlink;
    uint64_t ref_block_num;

    bool operator==(const mssgid& value) const {
        return author == value.author &&
               permlink == value.permlink &&
               ref_block_num == value.ref_block_num;
    }

    EOSLIB_SERIALIZE(mssgid, (author)(permlink)(ref_block_num))
};

struct archive_info_v1 {
    mssgid id;
    uint16_t level;
};

using archive_record = std::variant<archive_info_v1>;

struct messagestate {
    base_t netshares = 0;
    base_t voteshares = 0;
    base_t sumcuratorsw = 0;
};

struct message {
    message() = default;

    uint64_t id;
    std::string permlink;
    uint64_t ref_block_num;
    uint64_t date;
    name parentacc;
    uint64_t parent_id;
    base_t tokenprop; //elaf_t
    std::vector<structures::beneficiary> beneficiaries;
    base_t rewardweight;//elaf_t
    messagestate state;
    uint64_t childcount;
    uint16_t level;
    base_t curators_prcnt;

    uint64_t primary_key() const {
        return id;
    }
    
    std::tuple<std::string, uint64_t> secondary_key() const {
        return {permlink, ref_block_num};
    }
};

struct delegate_voter {
    delegate_voter() = default;

    name delegator;
    asset quantity;
    uint16_t interest_rate;
    uint8_t payout_strategy;
};

struct voteinfo {
    voteinfo() = default;

    uint64_t id;
    uint64_t message_id;
    name voter;
    int16_t weight;
    uint64_t time;
    int64_t count;
    std::vector<delegate_voter> delegators;
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
    base_t maxtokenprop; //elaf_t
    //uint64_t cashout_time; //TODO:
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
        if (r < 0) {
            return std::numeric_limits<ratio_t>::max();
        }
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

struct create_event {
    uint64_t record_id;
};

struct post_event {
    name author;
    std::string permlink;

    base_t netshares = 0;
    base_t voteshares = 0;
    base_t sumcuratorsw = 0;

    base_t sharesfn;
};

struct vote_event {
    name voter;
    name author;
    std::string permlink;
    int16_t weight;
    base_t curatorsw;
    base_t rshares;
};

struct pool_event {
    uint64_t created;
    counter_t msgs;
    eosio::asset funds;
    wide_t rshares;
    wide_t rsharesfn;
};

struct reward_weight_event {
    mssgid message_id;
    base_t rewardweight;
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

using id_index = indexed_by<N(primary), const_mem_fun<structures::message, uint64_t, &structures::message::primary_key>>;
using permlink_index = indexed_by<N(bypermlink), const_mem_fun<structures::message, std::tuple<std::string, uint64_t>, &structures::message::secondary_key>>;
using message_table = multi_index<N(message), structures::message, id_index, permlink_index>;

using vote_id_index = indexed_by<N(id), const_mem_fun<structures::voteinfo, uint64_t, &structures::voteinfo::primary_key>>;
using vote_messageid_index = indexed_by<N(messageid), const_mem_fun<structures::voteinfo, uint64_t, &structures::voteinfo::by_message>>;
using vote_group_index = indexed_by<N(byvoter), eosio::composite_key<structures::voteinfo, 
      eosio::member<structures::voteinfo, uint64_t, &structures::voteinfo::message_id>,
      eosio::member<structures::voteinfo, name, &structures::voteinfo::voter>>>;
using vote_table = multi_index<N(vote), structures::voteinfo, vote_id_index, vote_messageid_index, vote_group_index>;

using reward_pools = multi_index<N(rewardpools), structures::rewardpool>;
using limit_table = multi_index<N(limit), structures::limitparams>;

}


} // golos
