#include "golos.ctrl/golos.ctrl.hpp"
#include "golos.ctrl/config.hpp"
#include <golos.vesting/golos.vesting.hpp>
#include <eosio.system/native.hpp>
#include <eosio.token/eosio.token.hpp>
#include <eosiolib/transaction.hpp>
#include <common/dispatchers.hpp>

namespace golos {


using namespace eosio;
using std::vector;
using std::string;


/// properties
bool properties::validate() const {
    eosio_assert(is_account(owner), "owner not exists");
    eosio_assert(token.is_valid(), "invalid token");
    eosio_assert(max_witnesses > 0, "max_witnesses cannot be 0");
    eosio_assert(max_witness_votes > 0, "max_witness_votes cannot be 0");
    return true;
}

constexpr uint16_t calc_threshold(uint16_t val, uint16_t top, uint16_t num, uint16_t denom) {
    return 0 == val ? uint32_t(top) * num / denom + 1 : val;
}

uint16_t properties::super_majority_threshold() const { return calc_threshold(witness_supermajority, max_witnesses, 2, 3); };
uint16_t properties::majority_threshold() const { return calc_threshold(witness_majority, max_witnesses, 1, 2); };
uint16_t properties::minority_threshold() const { return calc_threshold(witness_minority, max_witnesses, 1, 3); };


////////////////////////////////////////////////////////////////
/// control
void control::assert_started() {
    eosio_assert(_props.exists(), "not initialized");
}

void control::create(properties new_props) {
    eosio_assert(!_props.exists(), "already created");
    eosio_assert(new_props.validate(), "invalid properties");
    require_auth(_self);
    _props.set(new_props, _self);
}

void control::updateprops(properties new_props) {
    assert_started();
    eosio_assert(new_props.validate(), "invalid properties");
    eosio_assert(props() != new_props, "same properties are already set");
    require_auth(_self);
    // TODO: auto-change auths on params change?
    _props.set(new_props, _self);
}

void control::on_transfer(name from, name to, asset quantity, string memo) {
    if (!_props.exists() || quantity.symbol != props().token)
        return; // distribute only community token

    if (to == _self && quantity.amount > 0) {
        // Don't check `from` for now, just distribute to top witnesses
        auto total = quantity.amount;
        auto top = top_witnesses();
        auto n = top.size();
        if (n == 0) {
            print("nobody in top");
            return;
        }

        auto token = props().token;
        auto transfer = [&](auto from, auto to, auto amount) {
            if (amount > 0) {
                INLINE_ACTION_SENDER(eosio::token, transfer)(config::token_name, {from, config::active_name},
                    {from, to, asset(amount, token), memo});
            }
        };
        static const auto memo = "emission";
        auto random = tapos_block_prefix();     // trx.ref_block_prefix; can generate hash from timestamp insead
        auto winner = top[random % n];          // witness, who will receive fraction after reward division
        auto reward = total / n;
        for (const auto& w: top) {
            if (w == winner)
                continue;
            transfer(_self, w, reward);
            total -= reward;
        }
        transfer(_self, winner, total);
    }
}


void control::attachacc(name user) {
    assert_started();
    require_auth(_self);
    upsert_tbl<bw_user_tbl>(user.value, [&](bool exists) {
        return [&,exists](bw_user& u) {
            eosio_assert(!exists || !u.attached, "already attached");   //TODO: maybe it's better to check this earlier (not inside modify())
            u.name = user;
            u.attached = true;
        };
    });
}

void control::detachacc(name user) {
    assert_started();
    require_auth(_self);
    bool exist = upsert_tbl<bw_user_tbl>(user.value, [&](bool) {
        return [&](bw_user& u) {
            eosio_assert(u.attached, "user already detached");
            u.attached = false;         // TODO: maybe delete?
        };
    }, false);
    eosio_assert(exist, "user not found");
}

void control::regwitness(name witness, eosio::public_key key, string url) {
    assert_started();
    eosio_assert(url.length() <= config::witness_max_url_size, "url too long");
    eosio_assert(key != eosio::public_key(), "public key should not be the default value");
    require_auth(witness);

    upsert_tbl<witness_tbl>(witness.value, [&](bool exists) {
        return [&,exists](witness_info& w) {
            eosio_assert(!exists || w.key != key || w.url != url, "already updated in the same way");
            w.name = witness;
            w.key = key;
            w.url = url;
            w.active = true;
        };
    });
    update_auths();
}

// TODO: special action to free memory?
void control::unregwitness(name witness) {
    assert_started();
    require_auth(witness);
    // TODO: simplify upsert to allow passing just inner lambda
    bool exists = upsert_tbl<witness_tbl>(witness.value, [&](bool) {
        return [&](witness_info& w) {
            eosio_assert(w.active, "witness already unregistered");
            w.active = false;
        };
    }, false);
    eosio_assert(exists, "witness not found");
    update_auths();
}

// Note: if not weighted, it's possible to pass all witnesses in vector like in BP actions
void control::votewitness(name voter, name witness, uint16_t weight/* = 10000*/) {
    // TODO: check witness existance (can work without it)
    assert_started();
    require_auth(voter);
    witness_vote_tbl tbl(_self, _self.value);
    auto itr = tbl.find(voter.value);
    bool exists = itr != tbl.end();

    auto update = [&](auto& v) {
        v.voter = voter;
        v.witnesses.emplace_back(witness);
    };
    if (exists) {
        auto& w = itr->witnesses;
        auto el = std::find(w.begin(), w.end(), witness);
        eosio_assert(el == w.end(), "already voted");
        eosio_assert(w.size() < props().max_witness_votes, "all allowed votes already casted");
        tbl.modify(itr, voter, update);
    } else {
        tbl.emplace(voter, update);
    }
    apply_vote_weight(voter, witness, true);
}


void control::unvotewitn(name voter, name witness) {
    assert_started();
    require_auth(voter);
    witness_vote_tbl tbl(_self, _self.value);
    auto itr = tbl.find(voter.value);
    bool exists = itr != tbl.end();
    eosio_assert(exists, "there are no votes");

    auto w = itr->witnesses;
    auto el = std::find(w.begin(), w.end(), witness);
    eosio_assert(el != w.end(), "there is no vote for this witness");
    w.erase(el);
    tbl.modify(itr, voter, [&](auto& v) {
        v.witnesses = w;
    });
    apply_vote_weight(voter, witness, false);
}

void control::changevest(name who, asset diff) {
    if (!_props.exists()) return;       // allow silent exit if changing vests before community created
    require_auth(config::vesting_name);
    eosio_assert(diff.amount != 0, "diff is 0. something broken");          // in normal conditions sender must guarantee it
    eosio_assert(diff.symbol.code() == props().token.code(), "wrong symbol. something broken");  // in normal conditions sender must guarantee it
    change_voter_vests(who, diff.amount);
}


////////////////////////////////////////////////////////////////////////////////

void control::change_voter_vests(name voter, share_type diff) {
    if (!diff) return;
    witness_vote_tbl tbl(_self, _self.value);
    auto itr = tbl.find(voter.value);
    bool exists = itr != tbl.end();
    if (exists && itr->witnesses.size()) {
        update_witnesses_weights(itr->witnesses, diff);
    }
}

void control::apply_vote_weight(name voter, name witness, bool add) {
    const auto power = vesting::get_account_vesting(config::vesting_name, voter, props().token.code()).amount;
    if (power > 0) {
        update_witnesses_weights({witness}, add ? power : -power);
    }
}

void control::update_witnesses_weights(vector<name> witnesses, share_type diff) {
    witness_tbl wtbl(_self, _self.value);
    for (const auto& witness : witnesses) {
        auto w = wtbl.find(witness.value);
        if (w != wtbl.end()) {
            wtbl.modify(w, witness, [&](auto& wi) {
                wi.total_weight += diff;            // TODO: additional checks of overflow? (not possible normally)
            });
        } else {
            // just skip unregistered witnesses (incl. non existing accs) for now
            print("apply_vote_weight: witness not found\n");
        }
    }
    update_auths();
}

void control::update_auths() {
    // TODO: change only if top changed #35
    auto top = top_witnesses();
    if (top.size() < props().max_witnesses) {           // TODO: ?restrict only just after creation and allow later
        print("Not enough witnesses to change auth\n");
        return;
    }
    eosiosystem::authority auth;
    for (const auto& i : top) {
        auth.accounts.push_back({{i,config::active_name},1});
    }

    vector<std::pair<name,uint16_t>> auths = {
        {config::minority_name, props().minority_threshold()},
        {config::majority_name, props().majority_threshold()},
        {config::super_majority_name, props().super_majority_threshold()}
    };

    const auto owner = props().owner;
    for (const auto& [perm, thrs]: auths) {
        //permissions must be sorted
        std::sort(auth.accounts.begin(), auth.accounts.end(),
            [](const eosiosystem::permission_level_weight& l, const eosiosystem::permission_level_weight& r) {
                return std::tie(l.permission.actor, l.permission.permission) <
                    std::tie(r.permission.actor, r.permission.permission);
            }
        );

        auth.threshold = thrs;
        action(
            permission_level{owner, config::active_name},
            config::internal_name, "updateauth"_n,
            std::make_tuple(owner, perm, config::active_name, auth)
        ).send();
    }
}

vector<witness_info> control::top_witness_info() {
    vector<witness_info> top;
    const auto l = props().max_witnesses;
    top.reserve(l);
    witness_tbl witness(_self, _self.value);
    auto idx = witness.get_index<"byweight"_n>();    // this index ordered descending
    for (auto itr = idx.begin(); itr != idx.end() && top.size() < l; ++itr) {
        if (itr->active && itr->total_weight > 0)
            top.emplace_back(*itr);
    }
    return top;
}

vector<name> control::top_witnesses() {
    const auto& wi = top_witness_info();
    vector<name> top(wi.size());
    std::transform(wi.begin(), wi.end(), top.begin(), [](auto& w) {
        return w.name;
    });
    return top;
}


} // namespace golos

DISPATCH_WITH_TRANSFER(golos::control, on_transfer,
    (create)(updateprops)
    (attachacc)(detachacc)
    (regwitness)(unregwitness)
    (votewitness)(unvotewitn)(changevest))
