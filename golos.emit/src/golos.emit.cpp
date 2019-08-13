#include "golos.emit/golos.emit.hpp"
#include <common/parameter_ops.hpp>
#include <eosio/transaction.hpp>
#include <eosio/time.hpp>
#include <cmath>

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
    std::optional<bwprovider> new_bwprovider;
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
    bool operator()(const bwprovider_param& p) {
        new_bwprovider = p;
        return set_param(p, &emit_state::bwprovider);
    }
};

void emission::validateprms(vector<emit_param> params) {
    //require_auth(who?)
    param_helper::check_params(params, _cfg.exists());
}

void emission::setparams(vector<emit_param> params) {
    require_auth(_self);
    auto setter = param_helper::set_parameters<emit_params_setter>(params, _cfg, _self);
    if (setter.new_bwprovider) {
        auto provider = setter.new_bwprovider->provider;
        if (provider.actor != name()) {
            dispatch_inline("cyber"_n, "providebw"_n, {provider}, std::make_tuple(provider.actor, _self));
        }
    }
}


void emission::start() {
    // TODO: disallow if no initial parameters set
    require_auth(_self);
    auto interval = cfg().interval.value;
    auto now = static_cast<uint64_t>(eosio::current_time_point().time_since_epoch().count());
    state s = {
        .start_time = now,
        .prev_emit = now - seconds(interval).count()   // instant emit. TODO: maybe delay on first start?
    };
    s = _state.get_or_create(_self, s);
    eosio::check(!s.active, "already active");
    s.active = true;

    uint32_t elapsed = microseconds(now - s.prev_emit).to_seconds();
    auto delay = elapsed < interval ? interval - elapsed : 0;
    _state.set(s, _self);
    schedule_next(delay);
}

void emission::stop() {
    require_auth(_self);
    auto s = _state.get();  // TODO: create own singleton allowing custom assert message
    eosio::check(s.active, "already stopped");
    s.active = false;
    _state.set(s, _self);
    cancel_deferred(cfg().token.symbol.raw());
}

int64_t emission::get_continuous_rate(int64_t annual_rate) {
    static auto constexpr real_100percent = static_cast<double>(config::_100percent);
    auto real_rate = static_cast<double>(annual_rate);
    return static_cast<int64_t>(std::log(1.0 + (real_rate / real_100percent)) * real_100percent); 
}

void emission::emit() {
    auto s = _state.get();
    eosio::check(s.active, "emit called in inactive state");    // impossible?
    auto now = eosio::current_time_point().time_since_epoch().count();
    auto elapsed = now - s.prev_emit;
    auto& interval = cfg().interval.value;
    auto emit_interval = seconds(interval).count();
    if (elapsed != emit_interval) {
        eosio::check(elapsed > emit_interval, "emit called too early");   // possible only on manual call
        print("warning: emit call delayed. elapsed: ", elapsed);
    }
    // TODO: maybe limit elapsed to avoid instant high value emission

    auto& token = cfg().token;
    auto& pools = cfg().pools.pools;
    auto& infrate = cfg().infrate;
    auto narrowed = microseconds(now - s.start_time).to_seconds() / infrate.narrowing;
    auto inf_rate = get_continuous_rate(std::max(int64_t(infrate.start) - narrowed, int64_t(infrate.stop)));

    auto supply = token::get_supply(config::token_name, token.symbol.code());
    auto issuer = token::get_issuer(config::token_name, token.symbol.code());
    auto new_tokens = static_cast<int64_t>(
        supply.amount * static_cast<uint128_t>(inf_rate) * time_to_blocks(elapsed)
        / (int64_t(config::blocks_per_year) * config::_100percent));

    if (new_tokens > 0) {
        const auto issue_memo = "emission"; // TODO: make configurable?
        const auto trans_memo = "emission";
        INLINE_ACTION_SENDER(token, issue)(config::token_name,
            {{issuer, config::issue_name}},
            {issuer, asset(new_tokens, token.symbol), issue_memo});

        auto transfer = [&](auto from, auto to, auto amount) {
            if (amount > 0) {
                auto memo = to == config::vesting_name ? "" : trans_memo;   // vesting contract requires empty memo to add to supply
                INLINE_ACTION_SENDER(token, transfer)(config::token_name, {from, config::issue_name},
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
            eosio::check(reward <= new_tokens, "SYSTEM: not enough tokens to pay emission reward"); // must not happen
            transfer(issuer, pool.name, reward);
            new_tokens -= reward;
        }
        eosio::check(remainder != name(), "SYSTEM: emission remainder pool is not set"); // must not happen
        transfer(issuer, remainder, new_tokens);
    } else {
        print("no emission\n");
    }

    s.prev_emit = now;
    _state.set(s, _self);
    schedule_next(interval);
}

void emission::schedule_next(uint32_t delay) {
    auto sender_id = cfg().token.symbol.raw();
    auto& provider = cfg().bwprovider.provider;
    auto& pools = cfg().pools.pools;

    transaction trx;
    trx.actions.emplace_back(action{permission_level(_self, config::code_name), _self, "emit"_n, std::tuple<>()});
    if (provider.actor != name()) {
        trx.actions.emplace_back(action{provider, "cyber"_n, "providebw"_n, std::make_tuple(provider.actor, _self)});
        for (const auto& pool: pools) {
            trx.actions.emplace_back(action{provider, "cyber"_n, "providebw"_n, std::make_tuple(provider.actor, pool)});
        }
    }
    trx.delay_sec = delay;
    trx.send(sender_id, _self);
}


} // golos

EOSIO_DISPATCH(golos::emission, (setparams)(validateprms)(emit)(start)(stop))
