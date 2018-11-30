#pragma once
#include <common/upsert.hpp>
#include <common/config.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/public_key.hpp>
#include <vector>
#include <string>

namespace golos {

using namespace eosio;
using share_type = int64_t;


struct [[eosio::table]] properties {
    // control
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

struct [[eosio::table]] ctrl_props {
    account_name owner;     // TODO: it should be singleton
    properties props;

    uint64_t primary_key() const {
        return owner;
    }
};
using props_tbl = eosio::multi_index<N(props), ctrl_props>;

struct [[eosio::table]] witness_info {
    account_name name;
    eosio::public_key key;
    std::string url;        // not sure it's should be in db (but can be useful to get witness info)
    bool active;            // can check key instead or even remove record

    uint64_t total_weight;

    uint64_t primary_key() const {
        return name;
    }
    uint64_t weight_key() const {
        return total_weight;     // TODO: resolve case where 2+ same weights exist (?extend key to 128bit)
    }
};
using witness_weight_idx = indexed_by<N(byweight), const_mem_fun<witness_info, uint64_t, &witness_info::weight_key>>;
using witness_tbl = eosio::multi_index<N(witness), witness_info, witness_weight_idx>;


struct [[eosio::table]] witness_voter_s {
    account_name community;
    std::vector<account_name> witnesses;

    uint64_t primary_key() const {
        return community;
    }
};
using witness_vote_tbl = eosio::multi_index<N(witnessvote), witness_voter_s>;


struct [[eosio::table]] bw_user {   // ?needed or simple names vector will be enough?
    account_name name;
    bool attached;          // can delete whole record on detach

    uint64_t primary_key() const {
        return name;
    }
};
using bw_user_tbl = eosio::multi_index<N(bwuser), bw_user>;


class control: public contract {
public:
    control(account_name self, symbol_type token = symbol_type(), uint64_t action = 0)
        : contract(self)
        , _token(token)
        , _props_tbl(_self, token)
    {
        props(action == N(create) || action == N(changevest) || action == 0);   // 0 = transfer
    };

    [[eosio::action]] void create(account_name owner, properties props);
    [[eosio::action]] void updateprops(properties props);

    [[eosio::action]] void attachacc(account_name user);
    [[eosio::action]] void detachacc(account_name user);
    inline bool is_attached(account_name user) const;

    [[eosio::action]] void regwitness(account_name witness, eosio::public_key key, std::string url);
    [[eosio::action]] void unregwitness(account_name witness);
    [[eosio::action]] void votewitness(account_name voter, account_name witness, uint16_t weight = 10000);
    [[eosio::action]] void unvotewitn(account_name voter, account_name witness);

    [[eosio::action]] void changevest(account_name owner, asset diff);
    void transfer(account_name from, account_name to, asset quantity, std::string memo);

    inline std::vector<account_name> get_top_witnesses();
    static inline std::vector<account_name> get_top_witnesses(symbol_type token, account_name code = config::control_name);
    static inline bool is_top_witness(account_name account, symbol_type token, account_name code = config::control_name);
    static inline properties get_params(symbol_type token);
    static inline account_name get_owner(symbol_type token);

private:
    symbol_type _token;
    account_name _owner;

    props_tbl _props_tbl;       // TODO: singleton
    bool _has_props = false;
    properties _props;          // cache

private:
    std::vector<account_name> top_witnesses();
    std::vector<witness_info> top_witness_info();

    const properties& props(bool allow_default = false) {
        if (!_has_props) {
            auto i = _props_tbl.begin();
            bool exists = i != _props_tbl.end();
            eosio_assert(exists || allow_default, "symbol not found");
            _props = exists ? i->props : properties();
            if (exists)
                _owner = i->owner;
            _has_props = exists;
        }
        return _props;
    }

    template<typename T, typename F>
    bool upsert_tbl(uint64_t scope, account_name payer, uint64_t key, F&& get_update_fn, bool allow_insert = true) {
        return golos::upsert_tbl<T>(_self, scope, payer, key, std::forward<F&&>(get_update_fn), allow_insert);
    }
    // common case where used ram of _owner account
    template<typename T, typename F>
    bool upsert_tbl(uint64_t key, F&& get_update_fn, bool allow_insert = true) {
        return upsert_tbl<T>(_token, _owner, key, std::forward<F&&>(get_update_fn), allow_insert);
    }

    void change_voter_vests(account_name voter, share_type diff);
    void apply_vote_weight(account_name voter, account_name witness, bool add);
    void update_witnesses_weights(std::vector<account_name> witnesses, share_type diff);
    void update_auths();

    // used by inline functions for external contracts
    static control& get_control(symbol_type token, account_name code = config::control_name) {
        static auto ctrl = control(code, token);
        return ctrl;
    }
};

bool control::is_attached(account_name user) const {
    bw_user_tbl tbl(_self, _owner);
    auto itr = tbl.find(user);
    bool exists = itr != tbl.end();
    return exists && itr->attached;
}

// separate helper for public usage
std::vector<account_name> control::get_top_witnesses() {
    std::vector<account_name> top;
    const auto l = props().max_witnesses;
    top.reserve(l);
    witness_tbl witness(_self, _token);
    auto idx = witness.get_index<N(byweight)>();
    for (auto itr = idx.begin(); itr != idx.end() && top.size() < l; ++itr) {
        if (itr->active && itr->total_weight > 0)
            top.emplace_back(itr->name);
    }
    return top;
}

std::vector<account_name> control::get_top_witnesses(symbol_type token, account_name code) {
    static auto top = get_control(token, code).get_top_witnesses();
    return top;
}
bool control::is_top_witness(account_name account, symbol_type token, account_name code) {
    auto t = get_top_witnesses(token, code);
    auto x = std::find(t.begin(), t.end(), account);
    return x != t.end();
}

properties control::get_params(symbol_type token) {
    return get_control(token)._props;
}
account_name control::get_owner(symbol_type token) {
    return get_control(token)._owner;
}


} // golos
