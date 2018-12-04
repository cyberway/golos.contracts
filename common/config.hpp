#pragma once

#ifndef UNIT_TEST_ENV
// test env still use N macro, so we continue to use it here
#   define STRINGIFY_(x) #x
#   define STRINGIFY(x) STRINGIFY_(x)
#   define N(x) eosio::name(STRINGIFY(x))
#endif


namespace golos { namespace config {

// contracts
static const auto control_name = N(gls.ctrl);
static const auto vesting_name = N(gls.vesting);
static const auto emission_name = N(gls.emit);
static const auto workers_name = N(gls.worker);
static const auto token_name = N(eosio.token);
static const auto internal_name = N(eosio);

// permissions
static const auto code_name = N(eosio.code);
static const auto owner_name = N(owner);
static const auto active_name = N(active);
static const auto invoice_name = N(invoice);
static const auto super_majority_name = N(witn.smajor);
static const auto majority_name = N(witn.major);
static const auto minority_name = N(witn.minor);

// numbers and time
static constexpr auto _1percent = 100;
static constexpr auto _100percent = 100 * _1percent;
static constexpr auto block_interval_ms = 3000;//1000 / 2;
static constexpr int64_t blocks_per_year = int64_t(365)*24*60*60*1000/block_interval_ms;

} // config

constexpr int64_t time_to_blocks(int64_t time) {
    return time / (1000 * config::block_interval_ms);
}
constexpr int64_t seconds_to_blocks(int64_t sec) {
    return time_to_blocks(sec * 1e6);
}

} // golos::config
