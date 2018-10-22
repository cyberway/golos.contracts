#pragma once
#include <eosiolib/eosio.hpp>
#include "types.h"
#include "config.hpp"
#include <common/calclib/atmsp_storable.h>
#include <eosiolib/time.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/singleton.hpp>
#include <eosiolib/crypto.h>
#include "charges.hpp"

using namespace eosio;

namespace structures {

struct accandvalue {
    account_name account;
    uint64_t value;
};
using counter_t = uint64_t;
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

struct messagestate {
    base_t absshares = 0;
    base_t netshares = 0;
    base_t voteshares = 0;
    base_t sumcuratorsw = 0;
};

struct message {
    message() = default;

    uint64_t id;
    uint64_t date;
    account_name parentacc;
    uint64_t parent_id;
    base_t tokenprop; //elaf_t
    std::vector<structures::beneficiary> beneficiaries;
    base_t rewardweight;//elaf_t
    messagestate state;
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
    
    base_t curatorsw;

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
  
struct poolstate {
    counter_t msgs;
    eosio::asset funds;    
    base_t rshares;
    base_t rsharesfn;    
    
    using ratio_t = decltype(fixp_t(funds.amount) / FP(rshares));
    ratio_t get_ratio() const {
        eosio_assert(funds.amount >= 0, "poolstate::get_ratio: funds < 0");
        auto r = FP(rshares);     
        return (r > 0) ? (fixp_t(funds.amount) / r) : std::numeric_limits<ratio_t>::max();
    }    
};

struct rewardpool {
    uint64_t created;
    rewardrules rules;
    limits lims;
    poolstate state;
    
    uint64_t primary_key() const { return created; }
    EOSLIB_SERIALIZE(rewardpool, (created)(rules)(lims)(state)) 
};

}

namespace tables {
    using id_index = indexed_by<N(id), const_mem_fun<structures::message, uint64_t, &structures::message::primary_key>>;
    using message_table = eosio::multi_index<N(messagetable), structures::message, id_index>;

    using content_id_index = indexed_by<N(id), const_mem_fun<structures::content, uint64_t, &structures::content::primary_key>>;
    using content_table = eosio::multi_index<N(contenttable), structures::content, content_id_index>;

    using vote_id_index = indexed_by<N(id), const_mem_fun<structures::voteinfo, uint64_t, &structures::voteinfo::primary_key>>;
    using vote_messageid_index = indexed_by<N(messageid), const_mem_fun<structures::voteinfo, uint64_t, &structures::voteinfo::by_message>>;
    using vote_table = eosio::multi_index<N(votetable), structures::voteinfo, vote_id_index, vote_messageid_index>;

    using reward_pools = eosio::multi_index<N(rewardpools), structures::rewardpool>;
    using charges = eosio::multi_index<N(charges), structures::usercharges>;
}
