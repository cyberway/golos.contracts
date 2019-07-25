#pragma once
#include <common/config.hpp>

namespace golos { namespace config {

constexpr int64_t max_cashout_time = 60 * 60 * 24 * 40; //40 days
constexpr size_t check_monotonic_steps = 10;
constexpr unsigned paymssgrwrd_expiration_sec = 3*60*60;
constexpr unsigned deletevotes_expiration_sec = 3*60*60;
constexpr unsigned closemssgs_expiration_sec  = 3*60*60;

constexpr unsigned closemssgs_sender_id = 1;

constexpr size_t target_payments_per_trx = 20;
constexpr size_t max_payments_per_trx    = 20;
constexpr size_t max_deletions_per_trx   = 100;

constexpr size_t max_closed_posts_per_action = 20;

static_assert(max_payments_per_trx >= target_payments_per_trx, "max_payments_per_trx < target_payments_per_trx");

namespace limit_restorer_domain {   // TODO: remove or rework (there are different cutoffs + postbw grows over cutoff)
    using namespace fixed_point_utils;
    constexpr fixp_t max_prev = fixp_t(400000);
    constexpr fixp_t max_vesting = std::numeric_limits<fixp_t>::max() / fixp_t(10);
    constexpr fixp_t max_elapsed_seconds = fixp_t(max_cashout_time);
    constexpr fixp_t max_res = fixp_t(400000);
}

const uint16_t max_length = 256;

}} // golos::config
