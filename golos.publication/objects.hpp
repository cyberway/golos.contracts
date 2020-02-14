#pragma once
#include <eosio/eosio.hpp>
#include "types.h"
#include "config.hpp"
#include <common/calclib/atmsp_storable.h>
#include <eosio/time.hpp>
#include <eosio/asset.hpp>
#include <eosio/singleton.hpp>
#include <eosio/crypto.hpp>

namespace golos { 

using percent_t = uint16_t;
using signed_percent_t = int16_t;

namespace structures {

using namespace eosio;

using counter_t = uint64_t;

struct beneficiary {
    name account;
    percent_t weight;
};

struct mssgid {
    mssgid() = default;

    name author;
    std::string permlink;

    bool operator==(const mssgid& value) const {
        return author == value.author &&
               permlink == value.permlink;
    }

    EOSLIB_SERIALIZE(mssgid, (author)(permlink))
};

struct messagestate {
    base_t netshares = 0;
    base_t voteshares = 0;
    base_t sumcuratorsw = 0;
};

struct message {
    message() = default;

    name author;
    uint64_t id;
    uint64_t date;
    uint64_t pool_date;
    percent_t tokenprop;
    std::vector<structures::beneficiary> beneficiaries;
    percent_t rewardweight;
    messagestate state;
    percent_t curators_prcnt;
    uint64_t cashout_time;
    eosio::asset mssg_reward;
    eosio::asset max_payout;
    int64_t paid_amount = 0;

    uint64_t primary_key() const {
        return id;
    }

    uint64_t by_cashout() const {
        return cashout_time;
    }

    bool closed() const {
        return cashout_time == microseconds::maximum().count();
    }
};

struct permlink {
    permlink() = default;

    uint64_t id;
    name parentacc;
    uint64_t parent_id;
    std::string value;
    uint16_t level;
    uint32_t childcount;

    uint64_t primary_key() const {
        return id;
    }

    std::string secondary_key() const {
        return value;
    }
};

struct permlink_info {
    mssgid msg;
    mssgid parent;
    uint16_t level;
    uint32_t childcount;
};

struct delegate_voter {
    delegate_voter() = default;

    name delegator;
    percent_t interest_rate;
};

struct voteinfo {
    voteinfo() = default;

    uint64_t id;
    uint64_t message_id;
    name voter;
    signed_percent_t weight;
    uint64_t time;
    uint8_t count;
    std::vector<delegate_voter> delegators;
    base_t curatorsw;
    base_t rshares;
    int64_t paid_amount = 0;

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
    percent_t maxtokenprop; // TODO: move to params #828
};

struct poolstate {
    counter_t msgs;
    eosio::asset funds;
    wide_t rshares;
    wide_t rsharesfn;

    using ratio_t = decltype(elap_t(1) / elap_t(1));
    ratio_t get_ratio() const {
        eosio::check(funds.amount >= 0, "poolstate::get_ratio: funds < 0");
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

struct [[using eosio: event("poststate"), contract("golos.publication")]] post_event {
    name author;
    std::string permlink;

    base_t netshares = 0;
    base_t voteshares = 0;
    base_t sumcuratorsw = 0;

    base_t sharesfn;
};

struct [[using eosio: event("votestate"), contract("golos.publication")]] vote_event {
    name voter;
    name author;
    std::string permlink;
    signed_percent_t weight;
    base_t curatorsw;
    base_t rshares;
};

struct [[using eosio: event("poolstate"), contract("golos.publication")]] pool_event {
    uint64_t created;
    counter_t msgs;
    eosio::asset funds;
    wide_t rshares;
    wide_t rsharesfn;
};

struct [[using eosio: event("poolerase"), contract("golos.publication")]] pool_erase_event {
    uint64_t created;
};

struct [[using eosio: event("rewardweight"), contract("golos.publication")]] reward_weight_event {
    mssgid message_id;
    percent_t rewardweight;
};

struct [[using eosio: event("postreward"), contract("golos.publication")]] post_reward_event {
    mssgid message_id;
    asset author_reward;
    asset benefactor_reward;
    asset curator_reward;
    asset unclaimed_reward;
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
        eosio::check(arg == "post bandwidth", "limitparams::act_from_str: wrong arg");
        return POSTBW;
    }
};

} // structures

namespace tables {

using namespace eosio;

using cashout_index [[using eosio: order("cashout_time","asc"), non_unique, contract("golos.publication")]] =
    indexed_by<N(bycashout), const_mem_fun<structures::message, uint64_t, &structures::message::by_cashout>>;
using message_table [[using eosio: order("id","asc"), contract("golos.publication")]] = multi_index<N(message), structures::message, cashout_index>;

using permlink_value_index [[using eosio: order("value","asc"), contract("golos.publication")]] =
    indexed_by<N(byvalue), const_mem_fun<structures::permlink, std::string, &structures::permlink::secondary_key>>;
using permlink_table [[using eosio: order("id","asc"), contract("golos.publication")]] = multi_index<N(permlink), structures::permlink, permlink_value_index>;

using vote_messageid_index [[using eosio: order("message_id","asc"), order("curatorsw","desc"), non_unique, contract("golos.publication")]] =
    indexed_by<N(messageid), eosio::composite_key<structures::voteinfo,
        eosio::member<structures::voteinfo, uint64_t, &structures::voteinfo::message_id>,
        eosio::member<structures::voteinfo, base_t, &structures::voteinfo::curatorsw>>>;
using vote_group_index [[using eosio: order("message_id","asc"), order("voter","asc"), contract("golos.publication")]] =
    indexed_by<N(byvoter), eosio::composite_key<structures::voteinfo,
        eosio::member<structures::voteinfo, uint64_t, &structures::voteinfo::message_id>,
        eosio::member<structures::voteinfo, name, &structures::voteinfo::voter>>>;
using vote_table [[using eosio: order("id","asc"), contract("golos.publication")]] = multi_index<N(vote), structures::voteinfo, vote_messageid_index, vote_group_index>;

using reward_pools [[using eosio: order("created","asc"), contract("golos.publication")]] = multi_index<N(rewardpools), structures::rewardpool>;
using limit_table [[using eosio: order("act","asc"), contract("golos.publication")]] = multi_index<N(limit), structures::limitparams>;

}


} // golos
