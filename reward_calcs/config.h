#pragma once
namespace golos { namespace config {
    
constexpr auto vesting_name = N(golos.vest);
constexpr size_t CHECK_MONOTONIC_STEPS = 10;
constexpr int ONE_HUNDRED_PERCENT = 10000;
constexpr fixed_point_utils::fixp_t MIN_VESTING_FOR_VOTE = fixed_point_utils::fixp_t(3000);

}}


