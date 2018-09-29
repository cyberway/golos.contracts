#pragma once
#include <common/upsert.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/public_key.hpp>
#include <vector>
#include <string>

namespace golos {

using namespace eosio;


struct [[eosio::table]] properties {
    uint16_t max_witnesses = 21;        // MAX_WITNESSES
    uint16_t max_witness_votes = 30;    // MAX_ACCOUNT_WITNESS_VOTES

    bool validate() const;

    friend bool operator==(const properties& a, const properties& b) {
        return memcmp(&a, &b, sizeof(properties)) == 0;
    }
    friend bool operator!=(const properties& a, const properties& b) {
        return !(a == b);
    }
};

struct [[eosio::table]] ctrl_props {
    // symbol_type symbol;  // it's now in scope
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
        return std::numeric_limits<uint64_t>::max() - total_weight;     // TODO: resolve case where 2+ same weights exist (?extend key to 128bit)
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

// weighted version
// struct [[eosio::table]] witness_vote {
//     account_name witness;
//     uint64_t weight;
// };

// struct [[eosio::table]] witness_voter_w {
//     account_name community;
//     std::vector<witness_vote> votes;

//     uint64_t primary_key() const {
//         return community;
//     }
// };
// using witness_voter = witness_voter_s;
using witness_vote_tbl = eosio::multi_index<N(witnessvote), witness_voter_s>;


struct [[eosio::table]] bw_user {   // ?needed or simple names vector will be enough?
    account_name name;
    // time_point_sec when;    // TODO: auto-expire can be added
    bool attached;          // can delete whole record on detach

    uint64_t primary_key() const {
        return name;
    }
};
using bw_user_tbl = eosio::multi_index<N(bwuser), bw_user>;

}


class control: public contract {
public:
    control(account_name self, symbol_type token, uint64_t action)
        : contract(self)
        , _token(token)
        , _props_tbl(_self, token)
    {
        props(action == N(create));
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

    [[eosio::action]] void updatetop(account_name from, account_name to, asset amount); // TODO: it's easier to receive final balances

    vector<account_name> top_witnesses();
    vector<witness_info> top_witness_info();  //internal

private:
    symbol_type _token;
    account_name _owner;

    props_tbl _props_tbl;       // TODO: singleton
    bool _has_props = false;    // maybe reuse _owner ?
    properties _props;          // cache

private:
    const properties& props(bool allow_default = false) {
        if (!_has_props) {
            auto i = _props_tbl.begin();
            bool exist = i != _props_tbl.end();
            eosio_assert(exist || allow_default, "symbol not found");
            _props = exist ? i->props : properties();
            if (exist)
                _owner = i->owner;
            _has_props = exist;
        }
        return _props;
    }

    template<typename T, typename F>
    bool upsert_tbl(uint64_t scope, account_name who, uint64_t key, F&& get_update_fn, bool allow_insert = true) {
        return golos::upsert_tbl<T>(_self, scope, who, key, std::forward<F&&>(get_update_fn), allow_insert);
    }
    // common case where used ram of _owner account
    template<typename T, typename F>
    bool upsert_tbl(uint64_t key, F&& get_update_fn, bool allow_insert = true) {
        return upsert_tbl<T>(_owner, _owner, key, std::forward<F&&>(get_update_fn), allow_insert);
    }

    void apply_vote_weight(account_name voter, account_name witness, bool add);
    void update_auths();

    bool has_witness_auth(uint8_t require);
    bool has_witness_active_auth();
    bool has_witness_majority();
    bool has_witness_minority();
};

bool control::is_attached(account_name user) const {
    bw_user_tbl tbl(_self, _owner);
    auto itr = tbl.find(user);
    bool exists = itr != tbl.end();
    return exists && itr->attached;
}


}
