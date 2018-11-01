#include "golos.emit/golos.emit.hpp"
#include "golos.emit/config.hpp"
#include <golos.ctrl/golos.ctrl.hpp>
// #include <golos.vesting/golos.vesting.hpp>
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

void emission::start() {
    require_auth(_owner);
    state s = {
        .post_name = _owner,
        // .work_name = p.workers_pool,
        .start_time = current_time(),
        .prev_emit = current_time() - config::emit_period*1000'000   // instant emit. TODO: maybe delay on first start?
    };
    s = _state.get_or_create(_owner, s);
    eosio_assert(!s.active, "already active");
    s.active = true;

    uint32_t elapsed = (current_time() - s.prev_emit) / 1e6;
    auto delay = elapsed < config::emit_period ? config::emit_period - elapsed : 0;
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
    if (elapsed != 1e6 * config::emit_period) {
        eosio_assert(elapsed > config::emit_period, "emit called too early");   // impossible?
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
#define TRANSFER(to, amount) if (amount > 0) { \
    INLINE_ACTION_SENDER(eosio::token, transfer)(config::token_name, {from, N(active)}, \
        {from, to, asset(amount, _token), "emission"}); \
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
                TRANSFER(w, witness_reward);
                content_reward -= witness_reward;
            }
        }

        TRANSFER(_owner, content_reward);
        TRANSFER(config::vesting_name, vesting_reward);
        TRANSFER(p.workers_pool, workers_reward);

#undef TRANSFER
    }

    s.prev_emit = current_time();
    schedule_next(s, config::emit_period);
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

APP_DOMAIN_ABI(golos::emission, (emit)(start)(stop))
