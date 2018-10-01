#pragma once

using fixp_t = expr_t;
constexpr fixp_t FP(base_t a) { return fixp_t::from_data(a); };

enum class payment_t: enum_t { TOKEN, VESTING }; //TODO: STABLE_TOKEN
