#pragma once
#include <eosiolib/eosio.hpp>

namespace golos { namespace config {

// contracts
static const auto vesting_name = N(golos.vest);        // TODO: there should be some global config for contract names, etc
static const auto internal_name = N(eosio);

//permissions
static const auto code_name = N(eosio.code);
static const auto owner_name = N(owner);
static const auto active_name = N(active);
static const auto majority_name = N(witn.major);
static const auto minority_name = N(witn.minor);

} } // golos::config
