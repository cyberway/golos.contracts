#pragma once

#if defined(UNIT_TEST_ENV)
#define FIXED_POINT_ASSERT(COND,  MESSAGE) if(!(COND)) { throw std::runtime_error((MESSAGE)); }
#else
#define FIXED_POINT_ASSERT(COND,  MESSAGE) eosio_assert((COND), (MESSAGE))
#endif

#include "fixed_point.h"

namespace fixed_point_utils{

using fixp_t = sg14::fixed_point<base_t, -fixed_point_fractional_digits>;
using wdfp_t = sg14::fixed_point<wide_t, -fixed_point_fractional_digits>;
using elap_t = sg14::elastic_fixed_point<fixp_t::integer_digits, fixp_t::fractional_digits, base_t>;
using elaf_t = sg14::fixed_point<sg14::elastic_integer<std::numeric_limits<base_t>::digits, base_t>, -(std::numeric_limits<base_t>::digits - 1)>;
using elai_t = sg14::fixed_point<sg14::elastic_integer<std::numeric_limits<base_t>::digits, base_t>, 0>;

constexpr fixp_t FP(base_t a) { return fixp_t::from_data(a); };
constexpr wdfp_t WP(wide_t a) { return wdfp_t::from_data(a); };
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
    using decay_t = typename std::remove_cv<typename std::remove_reference<T>::type>::type;
    static_assert((U(-1) < U(0)) == (decay_t(-1) < decay_t(0)), "fp_cast: T::is_signed != U::is_signed"); //is_signed doesn't work for fixed_point
    using comp_t = typename std::conditional<(_impl::int_digits<U>() > _impl::int_digits<decay_t>()), U, decay_t>::type;

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

template <typename U, typename T>
void narrow_down(U& lhs, T& rhs) {
    FIXED_POINT_ASSERT(lhs >= 0 && rhs >= 0, "narrow_down: wrong args");
    using narrow_t = typename std::conditional<(U::integer_digits < T::integer_digits), U, T>::type;
    using comp_t   = typename std::conditional<(U::integer_digits > T::integer_digits), U, T>::type;
    constexpr comp_t narrow_max = std::numeric_limits<narrow_t>::max();

    auto& lhs_data = const_cast<typename U::rep&>(lhs.data());
    auto& rhs_data = const_cast<typename T::rep&>(rhs.data());
    while ((static_cast<comp_t>(lhs) > narrow_max) || (static_cast<comp_t>(rhs) > narrow_max)) {
        lhs_data >>= 1;
        rhs_data >>= 1;
    }
}

}//fixed_point_utils
