#pragma once
#include "golos.ctrl/parameters.hpp"
#include <common/upsert.hpp>
#include <common/config.hpp>
#include <eosio/time.hpp>
#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/singleton.hpp>
#include <eosio/crypto.hpp>
#include <vector>
#include <string>

namespace golos {

using namespace eosio;
using share_type = int64_t;


struct [[eosio::table]] witness_info {
    name name;
    std::string url;        // not sure it's should be in db (but can be useful to get witness info)
    bool active;            // can check key instead or even remove record

    uint64_t total_weight;
    uint64_t counter_votes;

    uint64_t primary_key() const {
        return name.value;
    }
    uint64_t weight_key() const {
        return total_weight;
    }
};
using witness_weight_idx = indexed_by<"byweight"_n, const_mem_fun<witness_info, uint64_t, &witness_info::weight_key>>;
using witness_tbl = eosio::multi_index<"witness"_n, witness_info, witness_weight_idx>;


struct [[eosio::table]] witness_voter {
    name voter;
    std::vector<name> witnesses;

    uint64_t primary_key() const {
        return voter.value;
    }
};
using witness_vote_tbl = eosio::multi_index<"witnessvote"_n, witness_voter>;

struct [[eosio::table]] msig_auths {
    std::vector<name> witnesses;
    time_point_sec last_update;
};
using msig_auth_singleton = eosio::singleton<"msigauths"_n, msig_auths>;


class control: public contract {
public:
    control(name self, name code, datastream<const char*> ds)
        : contract(self, code, ds)
        , _cfg(_self, _self.value)
    {
    }

    [[eosio::action]] void validateprms(std::vector<ctrl_param>);
    [[eosio::action]] void setparams(std::vector<ctrl_param>);

    [[eosio::action]] void regwitness(name witness, std::string url);
    [[eosio::action]] void unregwitness(name witness);
    [[eosio::action]] void stopwitness(name witness);
    [[eosio::action]] void startwitness(name witness);
    [[eosio::action]] void votewitness(name voter, name witness);
    [[eosio::action]] void unvotewitn(name voter, name witness);

    [[eosio::action]] void changevest(name who, asset diff);
    void on_transfer(name from, name to, asset quantity, std::string memo);

private:
    ctrl_params_singleton _cfg;
    const ctrl_state& props() {
        static const ctrl_state cfg = _cfg.get();
        return cfg;
    }
    void assert_started();

    std::vector<name> top_witnesses();
    std::vector<witness_info> top_witness_info();

    template<typename T, typename F>
    bool upsert_tbl(uint64_t scope, name payer, uint64_t key, F&& get_update_fn, bool allow_insert = true) {
        return golos::upsert_tbl<T>(_self, scope, payer, key, std::forward<F&&>(get_update_fn), allow_insert);
    }
    template<typename T, typename F>
    bool upsert_tbl(name payer, F&& get_update_fn, bool allow_insert = true) {
        return upsert_tbl<T>(_self.value, payer, payer.value, std::forward<F&&>(get_update_fn), allow_insert);
    }

    void change_voter_vests(name voter, share_type diff);
    void apply_vote_weight(name voter, name witness, bool add);
    void update_witnesses_weights(std::vector<name> witnesses, share_type diff);
    void update_auths();
    void send_witness_event(const witness_info& wi);
    void active_witness(golos::name witness, bool flag);
};


} // golos
