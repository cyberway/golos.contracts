#include "golos.emit/golos.emit.hpp"
#include <common/parameter_ops.hpp>
#include <eosiolib/transaction.hpp>
#include <eosiolib/time.hpp>

namespace golos {

using namespace eosio;
using std::vector;


////////////////////////////////////////////////////////////////
/// emission
emission::emission(name self, name code, datastream<const char*> ds)
    : contract(self, code, ds)
    , _state(_self, _self.value)
    , _cfg(_self, _self.value)
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
    bool operator()(const emit_token_params& p) {
        return set_param(p, &emit_state::token);
    }
    bool operator()(const emit_interval_param& p) {
        return set_param(p, &emit_state::interval);
    }
};

void emission::validateprms(vector<emit_param> params) {
    //require_auth(who?)
    param_helper::check_params(params, _cfg.exists());
}

void emission::setparams(vector<emit_param> params) {
    require_auth(_self);
    param_helper::set_parameters<emit_params_setter>(params, _cfg, _self);
}


void emission::start() {
    // TODO: disallow if no initial parameters set
    require_auth(_self);
    auto interval = _cfg.get().interval.value;
    auto now = current_time();
    state s = {
        .start_time = now,
        .prev_emit = now - seconds(interval).count()   // instant emit. TODO: maybe delay on first start?
    };
    s = _state.get_or_create(_self, s);
    eosio_assert(!s.active, "already active");
    s.active = true;

    uint32_t elapsed = microseconds(now - s.prev_emit).to_seconds();
    auto delay = elapsed < interval ? interval - elapsed : 0;
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
    auto interval = _cfg.get().interval.value;
    auto emit_interval = seconds(interval).count();
    if (elapsed != emit_interval) {
        eosio_assert(elapsed > emit_interval, "emit called too early");   // possible only on manual call
        print("warning: emit call delayed. elapsed: ", elapsed);
    }
    // TODO: maybe limit elapsed to avoid instant high value emission

    auto token = _cfg.get().token;
    auto pools = _cfg.get().pools.pools;
    auto infrate = _cfg.get().infrate;
    auto narrowed = microseconds(now - s.start_time).to_seconds() / infrate.narrowing;
    auto inf_rate = std::max(int64_t(infrate.start) - narrowed, int64_t(infrate.stop));

    auto supply = eosio::token::get_supply(config::token_name, token.symbol.code());
    auto issuer = eosio::token::get_issuer(config::token_name, token.symbol.code());
    auto new_tokens = static_cast<int64_t>(
        supply.amount * static_cast<uint128_t>(inf_rate) * time_to_blocks(elapsed)
        / (int64_t(config::blocks_per_year) * config::_100percent));

    if (new_tokens > 0) {
        const auto issue_memo = "emission"; // TODO: make configurable?
        const auto trans_memo = "emission";
        auto from = _self;
        INLINE_ACTION_SENDER(eosio::token, issue)(config::token_name,
            {{issuer, config::active_name}},
            {_self, asset(new_tokens, token.symbol), issue_memo});

        auto transfer = [&](auto from, auto to, auto amount) {
            if (amount > 0) {
                auto memo = to == config::vesting_name ? "" : trans_memo;   // vesting contract requires empty memo to add to supply
                INLINE_ACTION_SENDER(eosio::token, transfer)(config::token_name, {from, config::active_name},
                    {from, to, asset(amount, token.symbol), memo});
            }
        };
        auto total = new_tokens;
        auto remainder = name();
        for (const auto& pool: pools) {
            if (pool.percent == 0) {
                remainder = pool.name;
                continue;
            }
            auto reward = total * pool.percent / config::_100percent;
            eosio_assert(reward <= new_tokens, "SYSTEM: not enough tokens to pay emission reward"); // must not happen
            transfer(from, pool.name, reward);
            new_tokens -= reward;
        }
        eosio_assert(remainder != name(), "SYSTEM: emission remainder pool is not set"); // must not happen
        transfer(from, remainder, new_tokens);
    } else {
        print("no emission\n");
    }

    s.prev_emit = now;
    schedule_next(s, interval);
}

void emission::schedule_next(state& s, uint32_t delay) {
    auto sender_id = (uint128_t(_cfg.get().token.symbol.raw()) << 64) | s.prev_emit;

    transaction trx;
    trx.actions.emplace_back(action{permission_level(_self, config::active_name), _self, "emit"_n, std::tuple<>()});
    trx.delay_sec = delay;
    trx.send(sender_id, _self);

    s.tx_id = sender_id;
    _state.set(s, _self);
}


} // golos

EOSIO_DISPATCH(golos::emission, (setparams)(validateprms)(emit)(start)(stop))
