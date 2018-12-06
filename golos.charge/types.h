#pragma once

#ifdef UNIT_TEST_ENV
#   define ABI_TABLE
    using int128_t = __int128;
    using uint128_t = __uint128_t;
    using uint8_t = unsigned char;
#else
#   define ABI_TABLE [[eosio::table]]
#endif

using enum_t = uint8_t;
using base_t = int64_t;// !if you change it -- don't forget about the abi-file
using wide_t = int128_t;
constexpr int fixed_point_fractional_digits = 12;

#if defined(UNIT_TEST_ENV)
#   include "../common/calclib/fixed_point_utils.h"
#else
#   include <common/calclib/fixed_point_utils.h>
#endif
