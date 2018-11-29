#include "golos.emit/golos.emit.hpp"
#include "golos.emit/config.hpp"
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
    , _cfg(_self, _self)
{
}

struct emit_params_setter: set_params_visitor<emit_state> {
    using set_params_visitor::set_params_visitor; // enable constructor

    bool operator()(const infrate_params& p) {
        return set_param(p, &emit_state::infrate);
    }
    bool operator()(const reward_pools_param& p) {
        return set_param(p, &emit_state::pools);
    }
};

void emission::validateprms(vector<emit_param> params) {
    //require_auth(who?)
    param_helper::check_params(params);
}

void emission::setparams(vector<emit_param> params) {
    require_auth(_self);
    param_helper::check_params(params);

    auto update = _cfg.exists();
    eosio_assert(update || params.size() == emit_state::params_count, "must provide all parameters in initial set");
    auto s = update ? _cfg.get() : emit_state{};
    auto setter = emit_params_setter(s);
    bool changed = false;
    for (const auto& param: params) {
        changed |= std::visit(setter, param);   // why we have no ||= ?
    }
    eosio_assert(changed, "at least one parameter must change");    // don't add actions, which do nothing
    _cfg.set(setter.state, _self);
}


void emission::start() {
    // TODO: disallow if no initial parameters set
    require_auth(_self);
    auto now = current_time();
    state s = {
        .start_time = now,
        .prev_emit = now - config::emit_interval*1000'000   // instant emit. TODO: maybe delay on first start?
    };
    s = _state.get_or_create(_self, s);
    eosio_assert(!s.active, "already active");
    s.active = true;

    uint32_t elapsed = (now - s.prev_emit) / 1e6;
    auto delay = elapsed < config::emit_interval ? config::emit_interval - elapsed : 0;
    schedule_next(s, delay);
}

void emission::stop() {
    require_auth(_self);
    auto s = _state.get();  // TODO: create own singleton allowing custom assert message
    eosio_assert(s.active, "already stopped");
    auto id = s.tx_id;
    s.active = false;
    s.tx_id = 0;
    _state.set(s, _self);
    if (id) {
        cancel_deferred(id);
    }
}

void emission::emit() {
    require_auth(_self);
    auto s = _state.get();
    eosio_assert(s.active, "emit called in inactive state");    // impossible?
    auto now = current_time();
    auto elapsed = now - s.prev_emit;
    auto emit_interval = seconds(config::emit_interval).count();
    if (elapsed != emit_interval) {
        eosio_assert(elapsed > emit_interval, "emit called too early");   // possible only on manual call
        print("warning: emit call delayed. elapsed: ", elapsed);
    }
    // TODO: maybe limit elapsed to avoid instant high value emission

    auto pools = _cfg.get().pools.pools;
    auto infrate = _cfg.get().infrate;
    auto narrowed = microseconds(now - s.start_time).to_seconds() / infrate.narrowing;
    auto inf_rate = std::max(int64_t(infrate.start) - narrowed, int64_t(infrate.stop));

    auto token = eosio::token(config::token_name);
    auto new_tokens = static_cast<int64_t>(
        token.get_supply(_token.name()).amount * static_cast<uint128_t>(inf_rate) * time_to_blocks(elapsed)
        / (int64_t(config::blocks_per_year) * config::_100percent));

    if (new_tokens > 0) {
        const auto issue_memo = "emission"; // TODO: make configurable?
        const auto trans_memo = "emission";
        auto from = _self;
        INLINE_ACTION_SENDER(eosio::token, issue)(config::token_name, {{_self, N(active)}},
            {from, asset(new_tokens, _token), issue_memo});

        auto transfer = [&](auto from, auto to, auto amount) {
            if (amount > 0) {
                auto memo = to == config::vesting_name ? "" : trans_memo;   // vesting contract requires empty memo to add to supply
                INLINE_ACTION_SENDER(eosio::token, transfer)(config::token_name, {from, N(active)}, \
                    {from, to, asset(amount, _token), memo}); \
            }
        };
        account_name remainder = N();
        for (const auto& pool: pools) {
            if (pool.percent == 0) {
                remainder = pool.name;
                continue;
            }
            auto reward = new_tokens * pool.percent / config::_100percent;
            eosio_assert(reward <= new_tokens, "SYSTEM: not enough tokens to pay emission reward"); // must not happen
            transfer(from, pool.name, reward);
            new_tokens -= reward;
        }
        eosio_assert(remainder != N(), "SYSTEM: emission remainder pool is not set"); // must not happen
        transfer(from, remainder, new_tokens);
    } else {
        print("no emission\n");
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
    _state.set(s, _self);
}


} // golos

APP_DOMAIN_ABI(golos::emission, (setparams)(validateprms)(emit)(start)(stop))
