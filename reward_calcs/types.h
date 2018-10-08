#pragma once

#if defined(UNIT_TEST_ENV)
using int128_t = __int128;
using uint128_t = __uint128_t;
using uint8_t = unsigned char;
#define FIXED_POINT_ASSERT(COND,  MESSAGE ) if(!(COND)) { throw std::runtime_error((MESSAGE)); }
#else
#define FIXED_POINT_ASSERT(COND,  MESSAGE ) eosio_assert((COND), (MESSAGE))
#endif

using enum_t = uint8_t;
using base_t = int64_t;// !if you change it -- don't forget about the abi-file

struct
#ifndef UNIT_TEST_ENV
[[eosio::table]] 
#endif
funcparams {
    std::string str;
    base_t maxarg;

};

#if defined(UNIT_TEST_ENV)
#include "../common/calclib/fixed_point.h"
#else
#include <common/calclib/fixed_point.h>
#endif

using fixp_t = sg14::fixed_point<base_t, -8>;
constexpr fixp_t FP(base_t a) { return fixp_t::from_data(a); };

enum class payment_t: enum_t { TOKEN, VESTING }; //TODO: STABLE_TOKEN


