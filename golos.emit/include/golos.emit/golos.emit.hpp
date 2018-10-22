#pragma once
#include <eosiolib/eosio.hpp>
#include <eosiolib/singleton.hpp>
#include <eosiolib/asset.hpp>


namespace golos {

using namespace eosio;

struct [[eosio::table]] state {
    uint64_t prev_emit;
    uint128_t tx_id;
    uint64_t start_time;
    account_name post_name;     // get from ctrl?
    account_name work_name;     // get from ctrl?
    bool active;
};
using state_singleton = eosio::singleton<N(state), state>;


class emission: public contract {
public:
    emission(account_name self, symbol_type token, uint64_t action = 0);

    [[eosio::action]] void emit();
    [[eosio::action]] void start();
    [[eosio::action]] void stop();

private:
    symbol_type _token;
    account_name _owner;
    state_singleton _state;

    void schedule_next(state& s, uint32_t delay);
};

} // golos
