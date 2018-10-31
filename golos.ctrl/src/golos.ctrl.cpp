#include "golos.ctrl/golos.ctrl.hpp"
#include "golos.ctrl/config.hpp"
#include <golos.vesting/golos.vesting.hpp>
#include <eosio.system/native.hpp>
#include <eosiolib/transaction.hpp>

#define DOMAIN_TYPE symbol_type
#include <common/dispatcher.hpp>

namespace golos {

using namespace eosio;
using std::vector;
using std::string;

#define MAX_WITNESS_URL_SIZE 256


/// properties
bool properties::validate() const {
    eosio_assert(max_witnesses > 0, "max_witnesses cannot be 0");
    eosio_assert(max_witness_votes > 0, "max_witness_votes cannot be 0");
    validate_emit_params();
    return true;
}

constexpr uint16_t calc_threshold(uint16_t val, uint16_t top, uint16_t num, uint16_t denom) {
    return 0 == val ? uint32_t(top) * num / denom + 1 : val;
}

uint16_t properties::active_threshold()   const { return calc_threshold(witness_supermajority, max_witnesses, 2, 3); };
uint16_t properties::majority_threshold() const { return calc_threshold(witness_majority, max_witnesses, 1, 2); };
uint16_t properties::minority_threshold() const { return calc_threshold(witness_minority, max_witnesses, 1, 3); };


////////////////////////////////////////////////////////////////
/// control
void control::create(account_name owner, properties new_props) {
    eosio_assert(!_has_props, "this token already created");
    eosio_assert(new_props.validate(), "invalid properties");
    require_auth(owner);
    // TODO: ensure that owner owns token
    // TODO: maybe check if eosio.code set properly
    _props_tbl.emplace(owner, [&](auto& r) {
        r.owner = owner;
        r.props = new_props;
    });
}

void control::updateprops(properties new_props) {
    eosio_assert(new_props.validate(), "invalid properties");
    eosio_assert(props() != new_props, "same properties are already set");
    require_auth({_owner, config::active_name});
    // TODO: auto-change auths on params change?
    upsert_tbl<props_tbl>(_owner, [&](bool) {
        return [&](auto& p) {
            p.props = new_props;
        };
    });
}

void control::attachacc(account_name user) {
    require_auth({_owner, config::minority_name});
    upsert_tbl<bw_user_tbl>(user, [&](bool exists) {
        return [&,exists](bw_user& u) {
            eosio_assert(!exists || !u.attached, "already attached");   //TODO: maybe it's better to check this earlier (not inside modify())
            u.name = user;
            u.attached = true;
        };
    });
}

void control::detachacc(account_name user) {
    require_auth({_owner, config::minority_name});
    bool exist = upsert_tbl<bw_user_tbl>(user, [&](bool) {
        return [&](bw_user& u) {
            eosio_assert(u.attached, "user already detached");
            u.attached = false;         // TODO: maybe delete?
        };
    }, false);
    eosio_assert(exist, "user not found");
}

void control::regwitness(account_name witness, eosio::public_key key, string url) {
    eosio_assert(url.length() <= MAX_WITNESS_URL_SIZE, "url too long");
    eosio_assert(key != eosio::public_key(), "public key should not be the default value");
    // TODO: check if key unique? Actually, key became unused now, can be removed
    require_auth(witness);

    upsert_tbl<witness_tbl>(_token, witness, witness, [&](bool exists) {
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
void control::unregwitness(account_name witness) {
    require_auth(witness);
    // TODO: simplify upsert to allow passing just inner lambda
    bool exists = upsert_tbl<witness_tbl>(_token, witness, witness, [&](bool) {
        return [&](witness_info& w) {
            eosio_assert(w.active, "witness already unregistered");
            w.active = false;
        };
    }, false);
    eosio_assert(exists, "witness not found");
    update_auths();
}

// Note: if not weighted, it's possible to pass all witnesses in vector like in BP actions
void control::votewitness(account_name voter, account_name witness, uint16_t weight/* = 10000*/) {
    // TODO: check witness existance (can work without it)
    require_auth(voter);
    witness_vote_tbl tbl(_self, voter);     // TODO: decide, is this scope better than others
    auto itr = tbl.find(_owner);
    bool exists = itr != tbl.end();

    auto update = [&](auto& v) {
        v.community = _owner;               // only changed on create, same on update
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


void control::unvotewitn(account_name voter, account_name witness) {
    require_auth(voter);
    witness_vote_tbl tbl(_self, voter);
    auto itr = tbl.find(_owner);
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

void control::changevest(account_name owner, asset diff) {
    if (!_has_props) return;        // allow silent exit if changing vests before community created
    require_auth(config::vesting_name);
    eosio_assert(diff.amount != 0, "diff is 0. something broken");          // in normal conditions sender must guarantee it
    eosio_assert(diff.symbol == _token, "wrong symbol. something broken");  // in normal conditions sender must guarantee it
    change_voter_vests(owner, diff.amount);
}


////////////////////////////////////////////////////////////////////////////////

void control::change_voter_vests(account_name voter, share_type diff) {
    if (!diff) return;
    witness_vote_tbl tbl(_self, voter);
    auto itr = tbl.find(_owner);
    bool exists = itr != tbl.end();
    if (exists && itr->witnesses.size()) {
        update_witnesses_weights(itr->witnesses, diff);
    }
}

void control::apply_vote_weight(account_name voter, account_name witness, bool add) {
    golos::vesting vc(config::vesting_name);
    const auto power = vc.get_account_vesting(voter, _token.name()).amount;
    if (power > 0) {
        update_witnesses_weights({witness}, add ? power : -power);
    }
}

void control::update_witnesses_weights(vector<account_name> witnesses, share_type diff) {
    witness_tbl wtbl(_self, _token);
    for (const auto& witness : witnesses) {
        auto w = wtbl.find(witness);
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

    std::vector<std::pair<uint64_t,uint16_t>> auths = {
        {config::minority_name, props().minority_threshold()},
        {config::majority_name, props().majority_threshold()},
        {config::active_name, props().active_threshold()}         // active must be the last because it adds eosio.code
    };

    for (const auto& a: auths) {
        auto perm = a.first;
        auto thrs = a.second;
        bool is_active = config::active_name == perm;
        if (is_active) {
            auth.accounts.push_back({{_self, config::code_name}, thrs});    // add eosio.code
        }
        //permissions must be sorted
        std::sort(auth.accounts.begin(), auth.accounts.end(),
            [](const eosiosystem::permission_level_weight& l, const eosiosystem::permission_level_weight& r) {
                return std::tie(l.permission.actor, l.permission.permission) < std::tie(r.permission.actor, r.permission.permission);
            }
        );
        auth.threshold = thrs;
        const auto& act = action(
            permission_level{_owner, config::active_name},
            config::internal_name, N(updateauth),
            std::make_tuple(_owner, perm, is_active ? config::owner_name : config::active_name, auth)
        );
        act.send();
    }
}

vector<witness_info> control::top_witness_info() {
    vector<witness_info> top;
    const auto l = props().max_witnesses;
    top.reserve(l);
    witness_tbl witness(_self, _token);
    auto idx = witness.get_index<N(byweight)>();    // this index ordered descending
    for (auto itr = idx.begin(); itr != idx.end() && top.size() < l; ++itr) {
        if (itr->active && itr->total_weight > 0)
            top.emplace_back(*itr);
    }
    return top;
}

vector<account_name> control::top_witnesses() {
    const auto& wi = top_witness_info();
    vector<account_name> top(wi.size());
    std::transform(wi.begin(), wi.end(), top.begin(), [](auto& w) {
        return w.name;
    });
    return top;
}


} // namespace golos

APP_DOMAIN_ABI(golos::control,
    (create)(updateprops)
    (attachacc)(detachacc)
    (regwitness)(unregwitness)
    (votewitness)(unvotewitn)(changevest))
