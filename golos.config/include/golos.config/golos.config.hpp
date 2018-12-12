#pragma once
#include "golos.config/parameters.hpp"
#include <golos.emit/parameters.hpp>
#include <eosiolib/eosio.hpp>


namespace golos {

using namespace eosio;


// NOTE: not finished yet, delayed until we require median, etc…
class configer: public contract {
public:
    using contract::contract;

    // own parameters
    [[eosio::action]] void validateprms(std::vector<cfg_param>);
    [[eosio::action]] void setparams(std::vector<cfg_param>);

    // base for recalculatable parameters
    [[eosio::action]] void updateparams(name, std::vector<cfg_param>);
    [[eosio::action]] void updateparamse(name, std::vector<emit_param>);    // TODO: maybe combine into 1 action accepting all params
    [[eosio::action]] void notifytop(std::vector<name>);

private:
    std::vector<name> get_top_witnesses();
    bool is_top_witness(name account);
    void recalculate_state(std::vector<emit_param>);
};

} // golos
