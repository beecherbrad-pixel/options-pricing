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
  // TODO (piece 4): solve bsm_price(solve_type, sigma) == time_value for sigma.
  (void)time_value;  // silence unused variable warnings until piece 4
  (void)solve_type;
  return nan_result(IvStatus::MaxIterationsExceeded);
}

}  // namespace optlib
