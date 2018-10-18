#pragma once
namespace golos { namespace config {
    
constexpr auto vesting_name = N(golos.vest);
constexpr size_t CHECK_MONOTONIC_STEPS = 10;
constexpr int64_t ONE_HUNDRED_PERCENT = 10000;
constexpr size_t MAX_BENEFICIARIES = 64;
constexpr int64_t MAX_CASHOUT_TIME = 60 * 60 * 24 * 40; //40 days

namespace limit_restorer_domain {//TODO: it will look better in a rule settings
    using namespace fixed_point_utils;
    constexpr fixp_t max_prev = fixp_t(100);
    constexpr fixp_t max_vesting = std::numeric_limits<fixp_t>::max() / fixp_t(10);
    constexpr fixp_t max_elapsed_seconds = fixp_t(MAX_CASHOUT_TIME);
    constexpr fixp_t max_res = fixp_t(100);
}

}}


