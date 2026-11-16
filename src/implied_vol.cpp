#include "optlib/implied_vol.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "optlib/black_scholes.hpp"

namespace optlib {

namespace {

void validate(double price, double spot, double strike, double T, double r, double q,
              const ImpliedVolOptions& opts) {
  // The comparisons are written so that NaN fails them: every comparison with
  // NaN is false, so `!(x > 0)` is true for NaN, whereas `x <= 0` would be false
  // and let the NaN through.
  if (!(spot > 0.0) || !std::isfinite(spot)) {
    throw std::invalid_argument("implied_vol: spot must be positive and finite");
  }
  if (!(strike > 0.0) || !std::isfinite(strike)) {
    throw std::invalid_argument("implied_vol: strike must be positive and finite");
  }
  if (!(T > 0.0) || !std::isfinite(T)) {
    throw std::invalid_argument("implied_vol: T must be positive and finite");
  }
  if (!std::isfinite(r) || !std::isfinite(q)) {
    throw std::invalid_argument("implied_vol: r and q must be finite");
  }
  if (!(opts.sigma_tolerance > 0.0) || !std::isfinite(opts.sigma_tolerance)) {
    throw std::invalid_argument("implied_vol: sigma_tolerance must be positive and finite");
  }
  if (!(opts.max_iter > 0)) {
    throw std::invalid_argument("implied_vol: max_iter must be positive");
  }
  if (!(opts.sigma_max > 0.0) || !std::isfinite(opts.sigma_max)) {
    throw std::invalid_argument("implied_vol: sigma_max must be positive and finite");
  }

  if (!std::isfinite(price)) {
    throw std::invalid_argument("implied_vol: price must be finite");
  }
}

ImpliedVolResult nan_result(IvStatus status) {
  return {std::numeric_limits<double>::quiet_NaN(), 0, status};
}

double initial_guess(double Sd, double Kd, double T) {
  // Manaster-Koehler inflection point of price vs sigma, floored at 0.1 (it is 0 at the money).
  double sigma_0 = std::sqrt(2 * std::abs(std::log(Sd / Kd)) / T);
  return std::max(sigma_0, 0.1);
}

}  // namespace
ImpliedVolResult implied_vol(OptionType type, double price, double spot, double strike, double T,
                             double r, double q, const ImpliedVolOptions& opts) {
  // Validate inputs
  validate(price, spot, strike, T, r, q, opts);
  // Bounds and classification
  const double Sd = spot * std::exp(-q * T);
  const double Kd = strike * std::exp(-r * T);
  const double lower = std::max(sign(type) * (Sd - Kd), 0.0);
  const double upper = (type == OptionType::Call) ? Sd : Kd;
  const double eps = 1e-14 * std::max(Sd, Kd);

  if (price < lower - eps) {
    return nan_result(IvStatus::PriceBelowLowerBound);
  }
  if (price <= lower + eps) {
    return {0.0, 0, IvStatus::PriceAtLowerBound};
  }
  if (price >= upper) {
    return nan_result(IvStatus::PriceAtOrAboveUpperBound);
  }
  // OTM conversion, then the stub
  const double time_value = price - lower;

  // ITM options are solved as their OTM counterpart: by put-call parity the
  // counterpart's price is exactly this option's time value at the same sigma.
  const bool itm = lower > 0.0;
  const OptionType solve_type =
      itm ? (type == OptionType::Call ? OptionType::Put : OptionType::Call) : type;

  const auto price_at = [&](double sigma) {
    return bsm_price(solve_type, BsmInputs{spot, strike, T, r, sigma, q});
  };
  const auto greeks_at = [&](double sigma) {
    return bsm_greeks(solve_type, BsmInputs{spot, strike, T, r, sigma, q});
  };

  if (price_at(opts.sigma_max) < time_value - eps) {
    return nan_result(IvStatus::SigmaAboveMax);
  }

  double sigma = std::min(initial_guess(Sd, Kd, T), opts.sigma_max);
  double sigma_lo = 0.0;
  double sigma_hi = opts.sigma_max;

  for (int i = 1; i <= opts.max_iter; i++) {
    const Greeks g = greeks_at(sigma);
    const double diff = g.price - time_value;

    if (diff == 0.0) return {sigma, i, IvStatus::Converged};

    if (diff > 0) {
      sigma_hi = sigma;
    } else {
      sigma_lo = sigma;
    }
    // Converged on the Newton step itself: a step below the tolerance can round
    // `next` onto the bracket edge, and the safeguard would then reject a correct
    // answer and bisect away from it.
    const double step = diff / g.vega;
    if (std::abs(step) < opts.sigma_tolerance) {
      return {sigma - step, i, IvStatus::Converged};
    }
    double next = sigma - step;
    if (!(next < sigma_hi && next > sigma_lo)) {
      next = 0.5 * (sigma_lo + sigma_hi);
    }

    if (std::abs(next - sigma) < opts.sigma_tolerance) return {next, i, IvStatus::Converged};
    if (sigma_hi - sigma_lo < opts.sigma_tolerance)
      return {0.5 * (sigma_lo + sigma_hi), i, IvStatus::Converged};
    sigma = next;
  }

  return {std::numeric_limits<double>::quiet_NaN(), opts.max_iter, IvStatus::MaxIterationsExceeded};
}

}  // namespace optlib
