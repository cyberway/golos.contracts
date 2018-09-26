#include "golos.ctrl/golos.ctrl.hpp"
#include <golos.vesting/golos.vesting.hpp>
#include <eosiolib/transaction.hpp>

#define DOMAIN_TYPE symbol_type
#include <common/dispatcher.hpp>

namespace golos {

using namespace eosio;
using std::vector;
using std::string;

#define MAX_URL_SIZE 256


void control::updateprops(properties new_props) {
    auto current = props(true);
    bool create = !_has_props;
    if (create) {
        require_auth(_owner);
    } else {
        eosio_assert(has_witness_majority(), "not enough witness signatures to change properties");
    }
    eosio_assert(create || current != new_props, "same properties are already set");
    upsert_tbl<props_tbl>(_owner, _owner, _token, [&](bool) {
        return [&](auto& p) {
            p.symbol = _token;
            p.props = new_props;
        };
    });
}

void control::attachacc(account_name user) {
    require_auth(_owner);
    //check rights, additional auths, maybe login restrictions (if creating new acc)
    upsert_tbl<bw_user_tbl>(user, [&](bool exists) {
        return [&](bw_user& u) {
            eosio_assert(!exists || u.attached, "already attached");   //TODO: maybe it's better to check this earlier (not inside modify())
            u.name = user;
            u.attached = true;
        };
    });
    // todo: create account and add auths
}

void control::detachacc(account_name user) {
    require_auth(_owner);
    bool exist = upsert_tbl<bw_user_tbl>(user, [&](bool) {
        return [&](bw_user& u) {
            eosio_assert(u.attached, "user already detached");
            u.attached = false;         // TODO: maybe delete?
        };
    }, false);
    eosio_assert(exist, "user not found");
    // todo: remove auths
}


void control::regwitness(account_name witness, eosio::public_key key, string url) {
    eosio_assert(url.length() < MAX_URL_SIZE, "url too long");
    eosio_assert(key != eosio::public_key(), "public key should not be the default value");
    require_auth(witness);

    upsert_tbl<witness_tbl>(_owner, witness, witness, [&](bool exists) {
        return [&](witness_info& w) {
            if (exists) {
                eosio_assert(w.key != key || w.url != url, "already updated in the same way");
            } else {
                w.name = witness;
            }
            w.key = key;
            w.url = url;
            w.active = true;
        };
    });
}


void control::unregwitness(account_name witness) {
    require_auth(witness);
    // TODO: simplify upsert to allow passing just inner lambda
    bool exists = upsert_tbl<witness_tbl>(_owner, witness, witness, [&](bool) {
        return [&](witness_info& w) {
            w.active = false;
        };
    }, false);
    eosio_assert(exists, "witness not found");
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

void control::updatetop(account_name from, account_name to, asset amount) {
    // cannot be called directly
}


////////////////////////////////////////////////////////////////////////////////

void control::apply_vote_weight(account_name voter, account_name witness, bool add) {
    witness_tbl wtbl(_self, _owner);
    auto w = wtbl.find(witness);
    if (w != wtbl.end()) {
        golos::vesting vc(N(golos::vesting));
        const auto power = vc.get_account_vesting(N(voter), _token).amount;    //get_balance accepts symbol_name
        wtbl.modify(w, _owner, [&](auto& wi) {
            wi.total_weight = wi.total_weight + (add ? power : -power);
        });
    } else {
        // just skip unregistered witnesses (incl. non existing accs) for now
    }
}

vector<witness_info> control::top_witness_info() {
    vector<witness_info> top;
    const auto l = props().max_witnesses;
    top.reserve(l);
    witness_tbl witness(_self, _owner);
    auto idx = witness.get_index<N(byweight)>();
    for (auto itr = idx.begin(); itr != idx.end() && top.size() < l; ++itr) {
        if (itr->active)
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

// void read_signatures() {
//     constexpr size_t max_stack_buffer_size = 512;
//     size_t size = transaction_size();
//     char* buffer = (char*)(max_stack_buffer_size < size ? malloc(size) : alloca(size));
//     read_transaction(buffer, size);
//     // signatures are cut before pack (signed_transaction -> transaction), so it's unusable
//     // transaction tx = fc::raw::unpack<transaction>(trx_data, trx_size);     // can be costly
//     //flat_set<public_key_type> kyes = tx.get_signature_keys()
// }

bool control::has_witness_auth(uint8_t require) {
    // alternatively we can try to use `has_auth`, but it provides no info about permission_level and used key
    auto signatures = vector<eosio::public_key>();    // todo: read_signatures
    const auto& top = top_witness_info();         //todo: return witness_info
    int got = 0;
    for (const auto& w: top) {
        for (const auto& s: signatures) {
            const auto& key = s;
            if (key == w.key) {
                got++;
                break;
            }
        }
        if (got >= require)
            return true;
    }
    return false;
}

bool control::has_witness_active_auth() {
    return has_witness_auth(props().max_witnesses * 2 / 3 + 1);
}
bool control::has_witness_majority() {
    return has_witness_auth(props().max_witnesses * 1 / 2 + 1);
}
bool control::has_witness_minority() {
    return has_witness_auth(props().max_witnesses * 1 / 3 + 1);
}



} // namespace golos

APP_DOMAIN_ABI(golos::control,
    (updateprops)
    (attachacc)(detachacc)
    (regwitness)(unregwitness)
    (votewitness)(unvotewitn)(updatetop))
