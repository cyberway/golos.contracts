#pragma once

#if defined(UNIT_TEST_ENV)
using int128_t = __int128;
using uint128_t = __uint128_t;
#define FIXED_POINT_ASSERT(COND,  MESSAGE ) if(!(COND)) { throw std::runtime_error((MESSAGE)); }
#else
#define FIXED_POINT_ASSERT(COND,  MESSAGE ) eosio_assert((COND), (MESSAGE))
#endif

#include "fixed_point.h"

using base_t = int64_t;// !if you change it -- don't forget about the abi-file
using expr_t = sg14::fixed_point<base_t, -8>;
