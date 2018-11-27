#pragma once
#include <common/parameter.hpp>
#include <common/config.hpp>
#include <eosiolib/singleton.hpp>

namespace golos {


// TODO: value can be used to change calculation type to median or some other type
template<const std::string* N>
struct base_threshold: parameter {
    uint16_t value;

    void validate() const override {
        // TODO: must check value realted to max_witnesses
        eosio_assert(value > 0, (std::string(*N) + " can't be 0").c_str());
    }
};

const std::string _name_pools = "emit_pools_threshold";     // just PoC; TODO: better way
using emit_pools_threshold_param = param_wrapper<base_threshold<&_name_pools>,1>;
const std::string _name_infrate = "emit_infrate_threshold";
using emit_infrate_threshold_param = param_wrapper<base_threshold<&_name_infrate>,1>;


using cfg_param = std::variant<emit_pools_threshold_param, emit_infrate_threshold_param>;

struct [[eosio::table]] cfg_state {
    emit_pools_threshold_param pools_threshold;
    emit_infrate_threshold_param infrate_threshold;

    static constexpr int params_count = 2;
};
using cfg_state_singleton = eosio::singleton<N(cfgparams), cfg_state>;


} // golos
