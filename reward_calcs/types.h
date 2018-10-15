#pragma once

using enum_t = uint8_t;
using base_t = int64_t;// !if you change it -- don't forget about the abi-file
constexpr int fixed_point_fractional_digits = 8;

struct
#ifndef UNIT_TEST_ENV
[[eosio::table]] 
#endif
funcparams {
    std::string str;
    base_t maxarg;
};

enum class payment_t: enum_t { TOKEN, VESTING }; //TODO: STABLE_TOKEN?

#if defined(UNIT_TEST_ENV)
#include "../common/calclib/fixed_point_utils.h"
#else
#include <common/calclib/fixed_point_utils.h>
#endif




