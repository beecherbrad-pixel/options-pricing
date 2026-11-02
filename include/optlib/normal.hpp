#pragma once

// Standard normal density and distribution function.
//
// These are defined `inline` in the header rather than in a .cpp file: they are
// tiny and called in hot loops, so we want the compiler to be able to inline
// them into every caller. `inline` on a function defined in a header also tells
// the linker that identical copies in several translation units are fine (the
// "one definition rule" would otherwise reject them).

#include <cmath>

namespace optlib {

// C++20 has std::numbers::inv_sqrtpi etc. We target C++17, so define our own.
inline constexpr double kInvSqrt2 = 0.70710678118654752440;    // 1 / sqrt(2)
inline constexpr double kInvSqrt2Pi = 0.39894228040143267794;  // 1 / sqrt(2 pi)

/// Standard normal probability density, phi(x) = exp(-x^2 / 2) / sqrt(2 pi).
[[nodiscard]] inline double norm_pdf(double x) noexcept {
  return kInvSqrt2Pi * std::exp(-0.5 * x * x);
}

/// Standard normal cumulative distribution function, N(x).
///
/// Written with erfc rather than the textbook 0.5 * (1 + erf(x / sqrt 2)).
/// For very negative x, erf(x / sqrt 2) is close to -1 and 1 + erf(...)
/// subtracts two nearly equal numbers, losing every significant digit
/// ("catastrophic cancellation"). erfc computes the small tail directly, so
/// N(-10) ~ 7.6e-24 comes out accurate instead of 0. Deep out-of-the-money
/// option prices and the implied-vol solver in Phase 2 depend on those tails.
[[nodiscard]] inline double norm_cdf(double x) noexcept { return 0.5 * std::erfc(-x * kInvSqrt2); }

}  // namespace optlib
