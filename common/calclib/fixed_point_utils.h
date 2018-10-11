#pragma once

#if defined(UNIT_TEST_ENV)
using int128_t = __int128;
using uint128_t = __uint128_t;
using uint8_t = unsigned char;
#define FIXED_POINT_ASSERT(COND,  MESSAGE) if(!(COND)) { throw std::runtime_error((MESSAGE)); }
#else
#define FIXED_POINT_ASSERT(COND,  MESSAGE) eosio_assert((COND), (MESSAGE))
#endif

#include "fixed_point.h"

namespace fixed_point_utils{

using fixp_t = sg14::fixed_point<base_t, -fixed_point_fractional_digits>;
using elap_t = sg14::elastic_fixed_point<fixp_t::integer_digits, fixp_t::fractional_digits, base_t>;
using elaf_t = sg14::fixed_point<sg14::elastic_integer<std::numeric_limits<base_t>::digits, base_t>, -(std::numeric_limits<base_t>::digits - 1)>;
using elai_t = sg14::fixed_point<sg14::elastic_integer<std::numeric_limits<base_t>::digits, base_t>, 0>;

constexpr fixp_t FP(base_t a) { return fixp_t::from_data(a); };
constexpr elaf_t ELF(base_t a) { return elaf_t::from_data(a); };

namespace _impl
{
template <typename T, std::enable_if_t<std::is_integral<T>::value>* = nullptr>
constexpr int int_digits() { return std::numeric_limits<T>::digits; }

template <typename T, std::enable_if_t<!std::is_integral<T>::value>* = nullptr>
constexpr int int_digits() { return T::integer_digits; }
}

template <typename U, typename T>
U fp_cast(T&& arg, bool restr = true) {
    static_assert((U(-1) < U(0)) == (T(-1) < T(0)), "fp_cast: T::is_signed != U::is_signed"); //is_signed doesn't work for fixed_point
    using comp_t = typename std::conditional<(_impl::int_digits<U>() > _impl::int_digits<typename std::remove_cv<typename std::remove_reference<T>::type>::type>()), U, T>::type;
    
    comp_t arg_comp(arg);        
    if(arg_comp < comp_t(-std::numeric_limits<U>::max())) {
        FIXED_POINT_ASSERT(!restr, "fp_cast: overflow");
        return -std::numeric_limits<U>::max();
    }
    if(arg_comp > comp_t( std::numeric_limits<U>::max())) {
        FIXED_POINT_ASSERT(!restr, "fp_cast: overflow");
        return  std::numeric_limits<U>::max();
    }    
    return U(std::forward<T>(arg));    
}

template <typename T>
int64_t int_cast(T&& arg) {
    return fp_cast<int64_t>(std::forward<T>(arg));
}

}//fixed_point_utils
