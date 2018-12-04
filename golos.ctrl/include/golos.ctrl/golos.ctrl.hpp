#pragma once
#include <common/upsert.hpp>
#include <common/config.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/singleton.hpp>
#include <eosiolib/public_key.hpp>
#include <vector>
#include <string>

namespace golos {

using namespace eosio;
using share_type = int64_t;


struct [[eosio::table]] properties {
    // control
    name owner;
    symbol token = symbol{"GLS",3};
    uint16_t max_witnesses = 21;            // MAX_WITNESSES
    uint16_t witness_supermajority = 0;     // 0 = auto
    uint16_t witness_majority = 0;          // 0 = auto
    uint16_t witness_minority = 0;          // 0 = auto
    uint16_t max_witness_votes = 30;        // MAX_ACCOUNT_WITNESS_VOTES

    // helpers
    bool validate() const;
    uint16_t super_majority_threshold() const;
    uint16_t majority_threshold() const;
    uint16_t minority_threshold() const;

    friend bool operator==(const properties& a, const properties& b) {
        return memcmp(&a, &b, sizeof(properties)) == 0;
    }
    friend bool operator!=(const properties& a, const properties& b) {
        return !(a == b);
    }
};
using props_singleton = eosio::singleton<"props"_n, properties>;

struct [[eosio::table]] witness_info {
    name name;
    eosio::public_key key;
    std::string url;        // not sure it's should be in db (but can be useful to get witness info)
    bool active;            // can check key instead or even remove record

    uint64_t total_weight;

    uint64_t primary_key() const {
        return name.value;
    }
    uint64_t weight_key() const {
        return total_weight;     // TODO: resolve case where 2+ same weights exist (?extend key to 128bit)
    }
};
using witness_weight_idx = indexed_by<"byweight"_n, const_mem_fun<witness_info, uint64_t, &witness_info::weight_key>>;
using witness_tbl = eosio::multi_index<"witness"_n, witness_info, witness_weight_idx>;


struct [[eosio::table]] witness_voter_s {
    name voter;
    std::vector<name> witnesses;

    uint64_t primary_key() const {
        return voter.value;
    }
};
using witness_vote_tbl = eosio::multi_index<"witnessvote"_n, witness_voter_s>;


struct [[eosio::table]] bw_user {   // ?needed or simple names vector will be enough?
    name name;
    bool attached;          // can delete whole record on detach

    uint64_t primary_key() const {
        return name.value;
    }
};
using bw_user_tbl = eosio::multi_index<"bwuser"_n, bw_user>;


class control: public contract {
public:
    control(name self, name code, datastream<const char*> ds)
        : contract(self, code, ds)
        , _props(_self, _self.value)
    {
    }

    [[eosio::action]] void create(properties props);
    [[eosio::action]] void updateprops(properties props);

    [[eosio::action]] void attachacc(name user);
    [[eosio::action]] void detachacc(name user);
    inline bool is_attached(name user) const;

    [[eosio::action]] void regwitness(name witness, eosio::public_key key, std::string url);
    [[eosio::action]] void unregwitness(name witness);
    [[eosio::action]] void votewitness(name voter, name witness, uint16_t weight = 10000);
    [[eosio::action]] void unvotewitn(name voter, name witness);

    [[eosio::action]] void changevest(name who, asset diff);
    void on_transfer(name from, name to, asset quantity, std::string memo);

private:
    props_singleton _props;
    properties props() {
        return _props.get();
    }
    void assert_started();

    std::vector<name> top_witnesses();
    std::vector<witness_info> top_witness_info();

    template<typename T, typename F>
    bool upsert_tbl(uint64_t scope, name payer, uint64_t key, F&& get_update_fn, bool allow_insert = true) {
        return golos::upsert_tbl<T>(_self, scope, payer, key, std::forward<F&&>(get_update_fn), allow_insert);
    }
    template<typename T, typename F>
    bool upsert_tbl(uint64_t key, F&& get_update_fn, bool allow_insert = true) {
        return upsert_tbl<T>(_self.value, _self, key, std::forward<F&&>(get_update_fn), allow_insert);
    }

    void change_voter_vests(name voter, share_type diff);
    void apply_vote_weight(name voter, name witness, bool add);
    void update_witnesses_weights(std::vector<name> witnesses, share_type diff);
    void update_auths();
};

bool control::is_attached(name user) const {
    bw_user_tbl tbl(_self, _self.value);
    auto itr = tbl.find(user.value);
    bool exists = itr != tbl.end();
    return exists && itr->attached;
}


} // golos
