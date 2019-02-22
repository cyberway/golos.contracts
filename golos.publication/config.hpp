#pragma once
#include <common/config.hpp>

namespace golos { namespace config {

constexpr int64_t max_cashout_time = 60 * 60 * 24 * 40; //40 days
constexpr size_t check_monotonic_steps = 10;

namespace limit_restorer_domain {//TODO: it will look better in a rule settings
    using namespace fixed_point_utils;
    constexpr fixp_t max_prev = fixp_t(100);
    constexpr fixp_t max_vesting = std::numeric_limits<fixp_t>::max() / fixp_t(10);
    constexpr fixp_t max_elapsed_seconds = fixp_t(max_cashout_time);
    constexpr fixp_t max_res = fixp_t(100);
}

const uint16_t max_length = 256;

}} // golos::config
