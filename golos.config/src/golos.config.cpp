#include <golos.config/golos.config.hpp>
#include <golos.config/config.hpp>
#include <common/parameter_ops.hpp>
#include <eosio/transaction.hpp>

namespace golos {

using namespace eosio;
using std::vector;


struct emit_params_setter: set_params_visitor<emit_state> {
    using set_params_visitor::set_params_visitor; // enable constructor

    bool operator()(const inflation_rate& p) {
        return set_param(p, &emit_state::infrate);
    }
    bool operator()(const reward_pools& p) {
        return set_param(p, &emit_state::pools);
    }
    bool operator()(const emit_token& p) {
        return set_param(p, &emit_state::token);
    }
    bool operator()(const emit_interval& p) {
        return set_param(p, &emit_state::interval);
    }
    bool operator()(const bwprovider& p) {
        return set_param(p, &emit_state::bwprovider);
    }
};

void configer::updateparamse(name who, std::vector<emit_param> params) {
    print("updateparams\n");
    eosio::check(who != _self, "can only change parameters of account, not contract");
    print("not self ok\n");
    require_auth(who);
    print("auth ok\n");
    param_helper::check_params(params, false); // TODO: validate using `validateprms` action of related contract
    // INLINE_ACTION_SENDER(contract_class, validateprms)(contract_name, {{_self, N(active)}}, {params});

    emit_params_singleton acc_params{_self, who.value};
    bool update = acc_params.exists();
    eosio::check(update || params.size() == emit_state::params_count,
        std::string("must provide all "+std::to_string(emit_state::params_count)+" parameters in initial set").c_str());
    auto current = update ? acc_params.get() : emit_state{};
    auto setter = emit_params_setter(current, update);
    for (const auto& param: params) {
        std::visit(setter, param);
    }
    acc_params.set(setter.state, who);

    if (is_top_witness(who)) {
        recalculate_state(params);
    }
}

struct emit_state_updater: state_params_update_visitor<emit_state> {
    using state_params_update_visitor::state_params_update_visitor;

    static const int THRESHOLD = 3; // test only; TODO: get from cfg_params_singleton

    void operator()(const inflation_rate& p) {
        update_state(&emit_state::infrate, THRESHOLD);
    }
    void operator()(const reward_pools& p) {
        update_state(&emit_state::pools, THRESHOLD);
    }
    void operator()(const emit_token& p) {
        update_state(&emit_state::token, THRESHOLD);
    }
};

vector<name> configer::get_top_witnesses() {
    vector<name> r;
    // TODO: get from singleton
    return r;
}

bool configer::is_top_witness(name account) {
    auto t = get_top_witnesses();
    auto x = std::find(t.begin(), t.end(), account);
    return x != t.end();
}

void configer::recalculate_state(vector<emit_param> changed_params) {
    emit_params_singleton state{_self, _self.value};
    auto s = state.exists() ? state.get() : emit_state{};   // TODO: default state must be created (in counstructor/init)

    auto top = get_top_witnesses();
    auto top_params = param_helper::get_top_params<emit_params_singleton, emit_state>(_self, top);
    auto v = emit_state_updater(s, top_params);
    for (const auto& param: changed_params) {
//        std::visit(v, param); // TODO: fix compilation
    }
    if (v.changed) {
        state.set(v.state, _self);
        // TODO: notify contract
        // INLINE_ACTION_SENDER(contract_class, setparams)(contract_name, {{_owner, N(active)}}, {v.state});

    }
}

void configer::notifytop(vector<name> top) {
    // TODO: 1. store new top to singleton; 2. recalculate state
    // require_auth(control);
}


////////////////////////////////////////////////////////////////
// own parameters
struct cfg_params_setter: set_params_visitor<cfg_state> {
    using set_params_visitor::set_params_visitor; // enable constructor

    bool operator()(const emit_pools_threshold_param& p) {
        return set_param(p, &cfg_state::pools_threshold);
    }
    bool operator()(const emit_infrate_threshold_param& p) {
        return set_param(p, &cfg_state::infrate_threshold);
    }
    bool operator()(const emit_token_symbol_threshold_param& p) {
        return set_param(p, &cfg_state::token_symbol_threshold);
    }
};

void configer::validateprms(vector<cfg_param> params) {
    cfg_params_singleton state{_self, _self.value};
    param_helper::check_params(params, state.exists());
}

void configer::setparams(vector<cfg_param> params) {
    require_auth(_self);
    validateprms(params);

    cfg_params_singleton state{_self, _self.value};
    auto s = state.exists() ? state.get() : cfg_state{};   // TODO: default state must be created (in counstructor/init)
    auto setter = cfg_params_setter(s);
    bool changed = false;
    for (const auto& param: params) {
        changed |= std::visit(setter, param);
    }
    eosio::check(changed, "at least one parameter must change");    // don't add actions, which do nothing
    state.set(setter.state, _self);
}


}

EOSIO_DISPATCH(golos::configer, (validateprms)(setparams)(notifytop));
