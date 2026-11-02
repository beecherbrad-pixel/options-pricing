#pragma once

// Black-Scholes-Merton prices and analytic Greeks for European options on an
// asset paying a continuous dividend yield q.
//
//   d1 = [ln(S/K) + (r - q + sigma^2 / 2) T] / (sigma sqrt T),   d2 = d1 - sigma sqrt T
//   call = S e^{-qT} N(d1)  - K e^{-rT} N(d2)
//   put  = K e^{-rT} N(-d2) - S e^{-qT} N(-d1)
//
// With q = 0 this is the original Black-Scholes model. Setting q = r gives
// Black's model for options on futures, which is what Deribit options are
// effectively priced with (Phase 5).
//
// Reference: Hull, "Options, Futures, and Other Derivatives", chapters on the
// Black-Scholes-Merton model and on the Greek letters.

#include "optlib/types.hpp"

namespace optlib {

/// Inputs to the Black-Scholes-Merton model.
///
/// Units (the usual source of bugs when connecting a model to market data):
///   spot, strike  in the same currency units
///   T             time to expiry in YEARS (e.g. 30 calendar days -> 30 / 365)
///   r, q          continuously compounded annual rates as decimals (0.05 = 5%)
///   sigma         annualised volatility as a decimal (0.20 = 20%, not 20)
///
/// An aggregate struct: no constructor, so it can be built with designated-style
/// braces in order: BsmInputs{spot, strike, T, r, q, sigma}.
struct BsmInputs {
  double spot;    // S: current price of the underlying
  double strike;  // K
  double T;       // time to expiry in years
  double r;       // risk-free rate
  double q;       // continuous dividend yield (or foreign rate for FX)
  double sigma;   // volatility
};

/// Price sensitivities. All are per unit change in the input, per year:
///
///   delta = dV/dS
///   gamma = d^2V/dS^2
///   vega  = dV/dsigma   per 1.00 of vol (a 100 vol-point move). Traders usually
///                       quote vega per 1 vol point: divide by 100.
///   theta = dV/dt = -dV/dT   per year of calendar time passing. Traders usually
///                       quote theta per day: divide by 365.
///   rho   = dV/dr        per 1.00 change in rate. Per basis point: divide by 10,000.
///
/// Hull reports vega, theta and rho in these same per-unit, per-year terms.
struct Greeks {
  double price;
  double delta;
  double gamma;
  double vega;
  double theta;
  double rho;
};

/// Black-Scholes-Merton price of a European option.
///
/// Edge cases:
///   T == 0 or sigma == 0: returns the limit of the formula, the discounted
///   payoff on the forward, e^{-rT} max(+-(F - K), 0) with F = S e^{(r-q)T}.
///   At T == 0 this is just intrinsic value.
///
/// @throws std::invalid_argument if spot <= 0, strike <= 0, T < 0, sigma < 0,
///         or any input is NaN or infinite.
[[nodiscard]] double bsm_price(OptionType type, const BsmInputs& in);

/// Price and all five Greeks in one call.
///
/// Cheaper than calling separate functions because d1, d2, N(d1), N(d2) and the
/// discount factors are shared by every Greek.
///
/// When T == 0 or sigma == 0 the Greeks of the deterministic limit are returned
/// (gamma = vega = 0; delta is +-e^{-qT} if in the money on a forward basis and 0
/// otherwise). Exactly at the money (F == K) the true delta is undefined; we
/// return 0 there.
///
/// @throws std::invalid_argument on invalid inputs (see bsm_price).
[[nodiscard]] Greeks bsm_greeks(OptionType type, const BsmInputs& in);

}  // namespace optlib
