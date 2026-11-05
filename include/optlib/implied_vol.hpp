#pragma once

#include "optlib/types.hpp"

namespace optlib {

/// Outcome of an implied-volatility solve.
enum class IvStatus {
  /// Solver converged; sigma holds the implied volatility.
  Converged,
  /// Price equals the no-arbitrage lower bound (no time value); sigma is exactly 0.
  PriceAtLowerBound,
  /// Price is below the no-arbitrage lower bound; no implied volatility exists.
  PriceBelowLowerBound,
  /// Price is at or above the no-arbitrage upper bound; implied volatility would be infinite.
  PriceAtOrAboveUpperBound,
  /// Price is valid, but its implied volatility exceeds ImpliedVolOptions::sigma_max.
  SigmaAboveMax,
  /// Solver did not reach sigma_tolerance within ImpliedVolOptions::max_iter iterations.
  MaxIterationsExceeded,
};

/// Result of implied_vol().
struct ImpliedVolResult {
  /// Implied volatility, annualized, as a decimal (0.20 = 20%).
  /// NaN unless status is Converged or PriceAtLowerBound.
  double sigma;
  /// Solver iterations performed (0 when no iteration was needed).
  int iterations;
  /// Whether the solve succeeded, and if not, why.
  IvStatus status;
};

/// Solver settings for implied_vol().
struct ImpliedVolOptions {
  /// Convergence tolerance on sigma: iteration stops when |delta sigma| falls below this.
  double sigma_tolerance = 1e-12;
  /// Maximum solver iterations before returning MaxIterationsExceeded.
  int max_iter = 100;
  /// Upper limit of the bracket search, as a decimal (10.0 = 1000% vol).
  double sigma_max = 10.0;
};

/// Computes the Black-Scholes-Merton implied volatility of a European option.
///
/// Finds the sigma at which bsm_price() matches `price`, using safeguarded Newton-Raphson
/// with a bisection fallback, started from the Manaster-Koehler point. In-the-money options
/// are converted to their out-of-the-money equivalent via put-call parity before solving.
///
/// Prices outside the no-arbitrage bounds are reported through ImpliedVolResult::status,
/// not by exception. Negative r and q are allowed.
///
/// @param type   Call or Put.
/// @param price  Observed option price.
/// @param spot   Spot price of the underlying (> 0).
/// @param strike Strike price (> 0).
/// @param T      Time to expiry in years (> 0).
/// @param r      Continuously compounded risk-free rate.
/// @param q      Continuously compounded dividend yield.
/// @param opts   Solver settings.
/// @throws std::invalid_argument if spot <= 0, strike <= 0, T <= 0,
///         or any input is NaN or infinite.
[[nodiscard]] ImpliedVolResult implied_vol(OptionType type, double price, double spot,
                                           double strike, double T, double r, double q,
                                           const ImpliedVolOptions& opts = {});

}  // namespace optlib
