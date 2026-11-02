#include "optlib/black_scholes.hpp"

#include <cmath>
#include <stdexcept>

#include "optlib/normal.hpp"

namespace optlib {

// An unnamed ("anonymous") namespace gives everything inside it internal
// linkage: these helpers are private to this .cpp file and can't clash with a
// function of the same name elsewhere. It's the C++ replacement for marking
// free functions `static`.
namespace {

void validate(const BsmInputs& in) {
  // The comparisons are written so that NaN fails them: every comparison with
  // NaN is false, so `!(x > 0)` is true for NaN, whereas `x <= 0` would be false
  // and let the NaN through.
  if (!(in.spot > 0.0) || !std::isfinite(in.spot)) {
    throw std::invalid_argument("bsm: spot must be positive and finite");
  }
  if (!(in.strike > 0.0) || !std::isfinite(in.strike)) {
    throw std::invalid_argument("bsm: strike must be positive and finite");
  }
  if (!(in.T >= 0.0) || !std::isfinite(in.T)) {
    throw std::invalid_argument("bsm: T must be non-negative and finite");
  }
  if (!(in.sigma >= 0.0) || !std::isfinite(in.sigma)) {
    throw std::invalid_argument("bsm: sigma must be non-negative and finite");
  }
  if (!std::isfinite(in.r) || !std::isfinite(in.q)) {
    throw std::invalid_argument("bsm: r and q must be finite");
  }
}

// Quantities shared by the price and every Greek. Computing them once in one
// place means the price function and the Greeks function can't drift apart.
struct Common {
  double w;           // +1 call, -1 put
  double df_r;        // e^{-rT}
  double df_q;        // e^{-qT}
  double s_fwd;       // S e^{-qT}: PV of receiving the stock at T
  double k_pv;        // K e^{-rT}: PV of paying the strike at T
  double vol_sqrt_t;  // sigma sqrt(T); zero means the outcome is deterministic
  double d1;          // only meaningful when vol_sqrt_t > 0
  double d2;
};

Common setup(OptionType type, const BsmInputs& in) {
  validate(in);
  Common c{};
  c.w = sign(type);
  c.df_r = std::exp(-in.r * in.T);
  c.df_q = std::exp(-in.q * in.T);
  c.s_fwd = in.spot * c.df_q;
  c.k_pv = in.strike * c.df_r;
  c.vol_sqrt_t = in.sigma * std::sqrt(in.T);
  if (c.vol_sqrt_t > 0.0) {
    c.d1 = (std::log(in.spot / in.strike) + (in.r - in.q + 0.5 * in.sigma * in.sigma) * in.T) /
           c.vol_sqrt_t;
    c.d2 = c.d1 - c.vol_sqrt_t;
  }
  return c;
}

// Greeks of the zero-volatility / zero-time limit. The payoff is then known for
// certain: the option is worth either nothing or the forward contract
// w (S e^{-qT} - K e^{-rT}).
Greeks deterministic_greeks(const Common& c, const BsmInputs& in) {
  const double forward_value = c.s_fwd - c.k_pv;
  if (c.w * forward_value <= 0.0) {
    return Greeks{0.0, 0.0, 0.0, 0.0, 0.0, 0.0};  // out of (or exactly at) the money
  }
  Greeks g{};
  g.price = c.w * forward_value;
  g.delta = c.w * c.df_q;
  g.gamma = 0.0;
  g.vega = 0.0;
  // theta = -dV/dT of  w (S e^{-qT} - K e^{-rT})
  g.theta = c.w * (in.q * c.s_fwd - in.r * c.k_pv);
  g.rho = c.w * in.T * c.k_pv;
  return g;
}

}  // namespace

double bsm_price(OptionType type, const BsmInputs& in) {
  // `const` on locals: computed once, never reassigned. It documents intent and
  // turns an accidental reassignment into a compile error.
  const Common c = setup(type, in);
  if (c.vol_sqrt_t == 0.0) {
    return std::fmax(c.w * (c.s_fwd - c.k_pv), 0.0);
  }
  // V = w [S e^{-qT} N(w d1) - K e^{-rT} N(w d2)]
  // For a call (w = +1) this is the textbook S e^{-qT} N(d1) - K e^{-rT} N(d2);
  // for a put (w = -1) it is K e^{-rT} N(-d2) - S e^{-qT} N(-d1).
  return c.w * (c.s_fwd * norm_cdf(c.w * c.d1) - c.k_pv * norm_cdf(c.w * c.d2));
}

Greeks bsm_greeks(OptionType type, const BsmInputs& in) {
  const Common c = setup(type, in);
  if (c.vol_sqrt_t == 0.0) {
    return deterministic_greeks(c, in);
  }

  const double nd1 = norm_cdf(c.w * c.d1);  // N(d1) for a call, N(-d1) for a put
  const double nd2 = norm_cdf(c.w * c.d2);
  const double pdf_d1 = norm_pdf(c.d1);  // phi is symmetric: no sign needed
  const double sqrt_t = std::sqrt(in.T);

  Greeks g{};
  g.price = c.w * (c.s_fwd * nd1 - c.k_pv * nd2);
  // Call delta lies in (0, e^{-qT}); put delta in (-e^{-qT}, 0).
  g.delta = c.w * c.df_q * nd1;
  // Gamma and vega are identical for calls and puts. By put-call parity,
  // call - put = S e^{-qT} - K e^{-rT}, which is linear in S and doesn't
  // involve sigma, so its second S-derivative and its sigma-derivative are 0.
  g.gamma = c.df_q * pdf_d1 / (in.spot * c.vol_sqrt_t);
  g.vega = c.s_fwd * pdf_d1 * sqrt_t;
  // Theta = time decay of optionality (always <= 0)
  //       - interest effect on the strike + dividend effect on the stock.
  g.theta = -c.s_fwd * pdf_d1 * in.sigma / (2.0 * sqrt_t)  //
            - c.w * in.r * c.k_pv * nd2                    //
            + c.w * in.q * c.s_fwd * nd1;
  g.rho = c.w * in.T * c.k_pv * nd2;
  return g;
}

}  // namespace optlib
