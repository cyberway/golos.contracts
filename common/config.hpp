#pragma once

namespace golos { namespace config {

// contracts
static const auto control_name = N(gls.ctrl);
static const auto vesting_name = N(gls.vesting);
static const auto emission_name = N(gls.emit);
static const auto publication_name = N(gls.publish);
static const auto token_name = N(eosio.token);
static const auto internal_name = N(eosio);

// permissions
static const auto code_name = N(eosio.code);
static const auto owner_name = N(owner);
static const auto active_name = N(active);
static const auto majority_name = N(witn.major);
static const auto minority_name = N(witn.minor);

// numbers and time
static constexpr auto _1percent = 100;
static constexpr auto _100percent = 100 * _1percent;
static constexpr auto block_interval_ms = 1000 / 2;
static constexpr int64_t blocks_per_year = int64_t(365)*24*60*60*1000/block_interval_ms;

} // config

constexpr int64_t time_to_blocks(int64_t time) {
    return time / (1000 * config::block_interval_ms);
}

} // golos::config
