#pragma once

// Interest-rate helpers shared by every pricing model.

namespace optlib {

/// Discount factor under continuous compounding: P(0, T) = exp(-r * T).
///
/// @param r  Continuously compounded risk-free rate, as a decimal (0.05 = 5%).
/// @param T  Time to maturity in years.
///
/// Finance convention: option models quote rates as *continuously compounded*
/// and time in *years*. Market data usually isn't quoted that way (e.g. a 5%
/// annual-compounding rate, or a maturity in calendar days), so convert first:
///   r_cont = ln(1 + r_annual),   T = days / 365 (ACT/365 day count).
[[nodiscard]] double discount_factor(double r, double T) noexcept;

}  // namespace optlib
