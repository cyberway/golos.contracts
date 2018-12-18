#pragma once
#include <common/config.hpp>
#include <eosiolib/asset.hpp>

namespace golos { namespace config {

static asset min_breakout = asset(0, symbol(symbol_code("GLS"), 4));
static asset max_breakout = asset(10000, symbol(symbol_code("GLS"), 4));

static const uint64_t min_expire = 0;
static const uint64_t max_expire = 10000;

static const uint64_t max_perсent = 8000;  // 80.00%

}} // golos::config
