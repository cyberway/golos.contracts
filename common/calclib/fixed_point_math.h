#pragma once
#include "fixed_point.h"
namespace sg14 {
template<class Rep, int Exponent>
constexpr fixed_point <Rep, Exponent>
//https://stackoverflow.com/questions/4657468/fast-fixed-point-pow-log-exp-and-sqrt
log2_unchecked (fixed_point <Rep, Exponent> arg) {
	using widened_base_t = set_digits_t<Rep, digits<Rep>::value * 2>;

	auto x = arg.data();
	constexpr int fract = fixed_point <Rep, Exponent>::fractional_digits;
	constexpr int displ = (fract > 0) ? 0 : 1 - fract;
	constexpr int prec  = (fract > 0) ? fract : 1;
	Rep b = 1 << (prec - 1);
	Rep y = 0;

	while (x < 1 << prec) {
		x <<= 1;
		y -= 1 << prec;
	}

	while (x >= 2 << prec) {
		x >>= 1;
		y += 1 << prec;
	}

	widened_base_t wide_raw = x;

	for (size_t i = 0; i < prec; i++) {
		wide_raw = wide_raw * wide_raw >> prec;
		if (wide_raw >= 2 << prec) 
		{
			wide_raw >>= 1;
			y += b;
		}
		b >>= 1;
	}
	return fixed_point <Rep, -prec>::from_data(y) + fixed_point <Rep, -prec>(displ);
}

template<class Rep, int Exponent>
fixed_point <Rep, Exponent>
log2 (fixed_point <Rep, Exponent> arg) {
	FIXED_POINT_ASSERT((arg > fixed_point <Rep, Exponent>(0)), "log2: arg <= 0");
	return log2_unchecked(arg);
}

template<class Rep, int Exponent>
fixed_point <Rep, Exponent>
log10 (fixed_point <Rep, Exponent> arg) {
	using T = fixed_point <Rep, Exponent>;
	FIXED_POINT_ASSERT((arg > static_cast<T>(0)), "log10: arg <= 0");
	
	constexpr T rev10log = static_cast<T>(1) / log2_unchecked(static_cast<T>(10));
	return (log2_unchecked(arg) * rev10log);
}

//TODO: pow, exp, sin etc.
}
