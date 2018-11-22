#include "golos.emit/golos.emit.hpp"
#include "golos.emit/config.hpp"
#include <golos.ctrl/golos.ctrl.hpp>
#include <eosio.token/eosio.token.hpp>
#include <eosiolib/transaction.hpp>

#define DOMAIN_TYPE symbol_type
#include <common/dispatcher.hpp>

namespace golos {

using namespace eosio;


////////////////////////////////////////////////////////////////
/// emission
emission::emission(account_name self, symbol_type token, uint64_t action)
    : contract(self)
    , _token(token)
    , _state(_self, token)
{
    _owner = control::get_owner(token);      // TODO: find better way
}


bool is_top_witness(account_name who, symbol_type token);

void emission::setparams(account_name who, std::vector<emit_param> params) {
    eosio_assert(who != _self, "can only change parameters of account, not contract");
    require_auth(who);
    param_helper::check_params(params);

    emit_state_singleton current_state{_self, who};
    eosio_assert(current_state.exists() || params.size() == emit_state::params_count,
        "must provide all parameters in initial set");
    auto first = !current_state.exists();
    auto current = !first ? current_state.get() : emit_state{};
    for (const auto& p: params) {
        switch (p.index()) {
        case 0: {
            auto ir = std::get<infrate_params>(p);
            eosio_assert(first || current.infrate != ir, "can't set same parameter value for infrate");
            current.infrate = ir;
            break;
        }
        case 1: {
            auto rp = std::get<reward_pools_param>(p);
            eosio_assert(first || current.pools != rp, "can't set same parameter value for pools");
            current.pools = rp;
            break;
        }
        default:
            eosio_assert(false, "SYSTEM: missing variant handler in emission::setparams");
        }
    }
    current_state.set(current, who);

    if (is_top_witness(who, _token)) {
        recalculate_state(params);
    }
}

bool is_top_witness(account_name who, symbol_type token) {
    auto ctrl = control(config::control_name, token);
    auto t = ctrl.get_top_witnesses();
    auto x = std::find(t.begin(), t.end(), who);
    return x != t.end();
}

struct state_update_visitor {
    emit_state& state;
    const std::vector<emit_state>& top;
    bool changed = false;

    state_update_visitor(emit_state& s, const std::vector<emit_state>& t): state(s), top(t) {}

    template<typename T>
    struct member_pointer_value;

    template<typename C, typename V>
    struct member_pointer_value<V C::*> {
        using type = V;
    };

    template<typename T, typename F>
    void update_state(F field) {
        static const int THRESHOLD = 2; // TODO: test only; actual value must be derived from max_witness_count
        bool ch = param_helper::update_state<T>(top, state, field, THRESHOLD);
        if (ch) {
            changed = true;
        }
    }

    void operator()(const inflation_rate& p) {
        print("processing infrate\n");
        update_state<infrate_params>(&emit_state::infrate);
    }
    void operator()(const reward_pools_param& p) {
        print("processing pools\n");
        update_state<reward_pools_param>(&emit_state::pools);
    }
};

std::vector<emit_state> get_top_params(symbol_type token, account_name code) {
    std::vector<emit_state> r;
    auto ctrl = control(config::control_name, token);
    auto top = ctrl.get_top_witnesses();
    for (const auto& w: top) {
        emit_state_singleton p{code, w};
        if (p.exists())
            r.emplace_back(p.get());
    }
    return r;
}

void emission::recalculate_state(std::vector<emit_param> changed_params) {
    emit_state_singleton state{_self, _self};
    auto s = state.exists() ? state.get() : emit_state{};   // TODO: default state must be created (in counstructor/init)
    auto top_params = get_top_params(_token, _self);
    auto v = state_update_visitor(s, top_params);
    for (const auto& param: changed_params) {
        std::visit(v, param);
    }
    if (v.changed) {
        state.set(v.state, _self);
        // TODO: notify
    }
}


void emission::start() {
    require_auth(_owner);
    state s = {
        .post_name = _owner,
        // .work_name = p.workers_pool,
        .start_time = current_time(),
        .prev_emit = current_time() - config::emit_interval*1000'000   // instant emit. TODO: maybe delay on first start?
    };
    s = _state.get_or_create(_owner, s);
    eosio_assert(!s.active, "already active");
    s.active = true;

    uint32_t elapsed = (current_time() - s.prev_emit) / 1e6;
    auto delay = elapsed < config::emit_interval ? config::emit_interval - elapsed : 0;
    schedule_next(s, delay);
}

void emission::stop() {
    require_auth(_owner);
    auto s = _state.get();  // TODO: create own singleton allowing custom assert message
    eosio_assert(s.active, "already stopped");
    auto id = s.tx_id;
    s.active = false;
    s.tx_id = 0;
    _state.set(s, _owner);
    if (id)
        cancel_deferred(id);
}

void emission::emit() {
    // eosio_assert(!_has_props, "this token already created");
    require_auth(_self);
    auto s = _state.get();
    eosio_assert(s.active, "emit called in inactive state");    // impossible?
    auto now = current_time();
    auto elapsed = now - s.prev_emit;
    if (elapsed != 1e6 * config::emit_interval) {
        eosio_assert(elapsed > config::emit_interval, "emit called too early");   // impossible?
        print("warning: emit call delayed. elapsed: ", elapsed);
    }
    // TODO: maybe limit elapsed to avoid instant high value emission

    auto p = control::get_params(_token);
    auto narrowed = int64_t((now - s.start_time)/1000000 / p.infrate_narrowing);
    int64_t infrate = std::max(int64_t(p.infrate_start) - narrowed, int64_t(p.infrate_stop));

    auto token = eosio::token(config::token_name);
    auto new_tokens =
        token.get_supply(_token.name()).amount * infrate * time_to_blocks(elapsed)
        / (int64_t(config::blocks_per_year) * config::_100percent);
    auto content_reward = new_tokens * p.content_reward / config::_100percent;
    auto vesting_reward = new_tokens * p.vesting_reward / config::_100percent;
    auto workers_reward = new_tokens * p.workers_reward / config::_100percent;
    auto witness_reward = new_tokens - content_reward - vesting_reward - workers_reward; /// Remaining 6% to witness pay
    eosio_assert(witness_reward >= 0, "bad reward percents params");    // impossible, TODO: remove after tests

    if (new_tokens > 0) {
#define TRANSFER(to, amount, memo) if (amount > 0) { \
    INLINE_ACTION_SENDER(eosio::token, transfer)(config::token_name, {from, N(active)}, \
        {from, to, asset(amount, _token), memo}); \
}
        auto from = _self;
        INLINE_ACTION_SENDER(eosio::token, issue)(config::token_name, {{_owner, N(active)}},
            {from, asset(new_tokens, _token), "emission"});

        // 1. witness reward can have remainder after division, it stay in content pool
        // 2. if there less than max witnesses, their reward stay in content pool
        content_reward += witness_reward;
        witness_reward /= p.max_witnesses;
        if (witness_reward > 0) {
            auto ctrl = control(config::control_name, _token);  // TODO: reuse existing control object
            auto top = ctrl.get_top_witnesses();
            for (const auto& w: top) {
                // TODO: maybe reimplement as claim to avoid missing balance asserts (or skip witnesses without balances)
                TRANSFER(w, witness_reward, "emission");
                content_reward -= witness_reward;
            }
        }

        TRANSFER(_owner, content_reward, "emission");
        TRANSFER(config::vesting_name, vesting_reward, ""); // memo must be empty to add to supply
        TRANSFER(p.workers_pool, workers_reward, "emission");

#undef TRANSFER
    }

    s.prev_emit = current_time();
    schedule_next(s, config::emit_interval);
}

void emission::schedule_next(state& s, uint32_t delay) {
    auto sender_id = (uint128_t(_token) << 64) | s.prev_emit;

    transaction trx;
    trx.actions.emplace_back(action{permission_level(_self, N(active)), _self, N(emit), _token});   // just use _token instead of struct
    trx.delay_sec = delay;
    trx.send(sender_id, _self);

    s.tx_id = sender_id;
    _state.set(s, _owner);
}


} // golos

APP_DOMAIN_ABI(golos::emission, (setparams)(emit)(start)(stop))
