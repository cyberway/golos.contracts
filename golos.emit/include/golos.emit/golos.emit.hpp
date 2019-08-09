#pragma once
#include "golos.emit/parameters.hpp"
#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <eosio/asset.hpp>


namespace golos {

using namespace eosio;

struct [[eosio::table]] state {
    uint64_t prev_emit;
    uint64_t start_time;
    bool active;
};
using state_singleton = eosio::singleton<"state"_n, state>;


class emission: public contract {
public:
    emission(name self, name, datastream<const char*>);

    [[eosio::action]] void validateprms(std::vector<emit_param>);
    [[eosio::action]] void setparams(std::vector<emit_param>);

    [[eosio::action]] void emit();
    [[eosio::action]] void start();
    [[eosio::action]] void stop();

private:
    void recalculate_state(std::vector<emit_param>);
    state_singleton _state;
    emit_params_singleton _cfg;

    const emit_state& cfg() {
        static const emit_state cfg = _cfg.get();
        return cfg;
    }

    void schedule_next(uint32_t delay);
};

} // golos
