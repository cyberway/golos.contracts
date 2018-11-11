#pragma once
#include <common/config.hpp>

namespace golos { namespace config {

const auto max_vote_changes = 5;

// closing post params
const auto cashout_window = 120;    // 7*24*60*60      // CASHOUT_WINDOW_SECONDS
const auto upvote_lockout = 15;     // 60*60           // UPVOTE_LOCKOUT

constexpr size_t max_beneficiaries = 64;                    // MAX_COMMENT_BENEFICIARIES
constexpr int64_t max_cashout_time = 60 * 60 * 24 * 40; //40 days
constexpr uint16_t max_comment_depth = 127;                 // 0xfff0 + ? SOFT_MAX_COMMENT_DEPTH 0xff

constexpr size_t check_monotonic_steps = 10;


namespace limit_restorer_domain {//TODO: it will look better in a rule settings
    using namespace fixed_point_utils;
    constexpr fixp_t max_prev = fixp_t(100);
    constexpr fixp_t max_vesting = std::numeric_limits<fixp_t>::max() / fixp_t(10);
    constexpr fixp_t max_elapsed_seconds = fixp_t(max_cashout_time);
    constexpr fixp_t max_res = fixp_t(100);
}

}} // golos::config
