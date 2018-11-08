#pragma once
#include <common/config.hpp>

namespace golos { namespace config {
const auto VOTE_OPERATION_INTERVAL = 3; //sec
const auto MAX_REVOTES = 5;

// closing post params
const auto CLOSE_MESSAGE_PERIOD = 120; // 7*24*60*60
const auto UPVOTE_DISABLE_PERIOD = 6; // 60*60

constexpr size_t CHECK_MONOTONIC_STEPS = 10;
constexpr int64_t ONE_HUNDRED_PERCENT = 10000;
constexpr size_t MAX_BENEFICIARIES = 64;
constexpr int64_t MAX_CASHOUT_TIME = 60 * 60 * 24 * 40; //40 days
constexpr uint16_t MAX_COMMENT_DEPTH = 127;

namespace limit_restorer_domain {//TODO: it will look better in a rule settings
    using namespace fixed_point_utils;
    constexpr fixp_t max_prev = fixp_t(100);
    constexpr fixp_t max_vesting = std::numeric_limits<fixp_t>::max() / fixp_t(10);
    constexpr fixp_t max_elapsed_seconds = fixp_t(MAX_CASHOUT_TIME);
    constexpr fixp_t max_res = fixp_t(100);
}

}} // golos::config
