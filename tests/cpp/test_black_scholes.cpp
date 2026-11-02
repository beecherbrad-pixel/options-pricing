// Correctness tests for the Black-Scholes-Merton price and Greeks.
//
// Four independent kinds of evidence:
//   1. Textbook values (Hull), at the precision Hull prints.
//   2. High-precision reference values computed separately with mpmath at 30
//      significant digits, checked to ~1e-12 relative error.
//   3. Model-free identities that must hold for any correct implementation:
//      put-call parity, no-arbitrage bounds, monotonicity.
//   4. Analytic Greeks vs finite-difference bump-and-reprice.

#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>
#include <vector>

#include "optlib/black_scholes.hpp"

namespace {

using optlib::bsm_greeks;
using optlib::bsm_price;
using optlib::BsmInputs;
using optlib::Greeks;
using optlib::OptionType;

constexpr auto kCall = OptionType::Call;
constexpr auto kPut = OptionType::Put;

// Relative-or-absolute closeness: |a - b| <= tol * max(1, |b|).
// Prices range from ~1e-118 to ~100, so neither pure absolute nor pure
// relative tolerance works across the board.
::testing::AssertionResult Close(double actual, double expected, double tol) {
  const double scale = std::fmax(1.0, std::fabs(expected));
  if (std::fabs(actual - expected) <= tol * scale) {
    return ::testing::AssertionSuccess();
  }
  return ::testing::AssertionFailure()
         << "actual " << actual << " vs expected " << expected << " (diff " << actual - expected
         << ", tol " << tol * scale << ")";
}

// ---------------------------------------------------------------------------
// 1. Hull textbook examples
// ---------------------------------------------------------------------------

// Hull, Black-Scholes-Merton chapter: S=42, K=40, r=10%, sigma=20%, T=0.5.
// Hull reports call = 4.76, put = 0.81.
TEST(BsmHull, PricingExample) {
  const BsmInputs in{42.0, 40.0, 0.5, 0.10, 0.0, 0.20};
  EXPECT_NEAR(bsm_price(kCall, in), 4.76, 0.005);
  EXPECT_NEAR(bsm_price(kPut, in), 0.81, 0.005);
}

// Hull, Greek letters chapter: a 20-week call with S=49, K=50, r=5%, sigma=20%.
// Hull reports price 2.40, delta 0.522, gamma 0.066, vega 12.1,
// theta -4.31 per year, rho 8.91.
TEST(BsmHull, GreeksExample) {
  const BsmInputs in{49.0, 50.0, 20.0 / 52.0, 0.05, 0.0, 0.20};
  const Greeks g = bsm_greeks(kCall, in);
  EXPECT_NEAR(g.price, 2.40, 0.005);
  EXPECT_NEAR(g.delta, 0.522, 0.0005);
  EXPECT_NEAR(g.gamma, 0.066, 0.0005);
  EXPECT_NEAR(g.vega, 12.1, 0.05);
  EXPECT_NEAR(g.theta, -4.31, 0.005);
  EXPECT_NEAR(g.rho, 8.91, 0.005);
}

// ---------------------------------------------------------------------------
// 2. High-precision references (mpmath, 30 digits; see tests/python for the
//    script-free SciPy cross-check)
// ---------------------------------------------------------------------------

struct ReferenceCase {
  OptionType type;
  BsmInputs in;
  Greeks expected;
};

const std::vector<ReferenceCase>& reference_cases() {
  // A function-local static is built once, on first use. Unlike a global, it
  // can't be used before it's initialised (the "static initialisation order
  // fiasco").
  static const std::vector<ReferenceCase> cases{
      {kCall,
       {42.0, 40.0, 0.5, 0.10, 0.0, 0.20},
       {4.7594223928715334, 0.77913129094266894, 0.049962670405911853, 8.8134150596028514,
        -4.5590921945926267, 13.982045913360281}},
      {kPut,
       {42.0, 40.0, 0.5, 0.10, 0.0, 0.20},
       {0.80859937290009365, -0.22086870905733106, 0.049962670405911853, 8.8134150596028514,
        -0.7541744965897705, -5.0425425766539992}},
      {kCall,
       {49.0, 50.0, 20.0 / 52.0, 0.05, 0.0, 0.20},
       {2.4005273232717147, 0.52160466106639639, 0.065544039347844394, 12.1054798826288,
        -4.3053298229325736, 8.9069619496083495}},
      {kPut,
       {49.0, 50.0, 20.0 / 52.0, 0.05, 0.0, 0.20},
       {2.4481754412818496, -0.47839533893360361, 0.065544039347844394, 12.1054798826288,
        -1.8529474170320668, -9.957518095780164}},
      // With a dividend yield.
      {kCall,
       {100.0, 95.0, 0.75, 0.03, 0.02, 0.35},
       {14.554902082095149, 0.6256928811150684, 0.01221441717466344, 32.062845083491528,
        -7.67037633813357, 36.010789522058769}},
      {kPut,
       {100.0, 95.0, 0.75, 0.03, 0.02, 0.35},
       {8.9300756551558373, -0.35941905848799426, 0.01221441717466344, 32.062845083491528,
        -6.8540091913386868, -33.653986127966447}},
  };
  return cases;
}

TEST(BsmReference, PriceAndGreeksMatchHighPrecision) {
  for (const auto& c : reference_cases()) {
    // SCOPED_TRACE adds context to any failure inside this loop iteration.
    SCOPED_TRACE(::testing::Message() << (c.type == kCall ? "call" : "put") << " S=" << c.in.spot
                                      << " K=" << c.in.strike);
    const Greeks g = bsm_greeks(c.type, c.in);
    EXPECT_TRUE(Close(g.price, c.expected.price, 1e-12));
    EXPECT_TRUE(Close(g.delta, c.expected.delta, 1e-12));
    EXPECT_TRUE(Close(g.gamma, c.expected.gamma, 1e-12));
    EXPECT_TRUE(Close(g.vega, c.expected.vega, 1e-12));
    EXPECT_TRUE(Close(g.theta, c.expected.theta, 1e-12));
    EXPECT_TRUE(Close(g.rho, c.expected.rho, 1e-12));
    // bsm_price and bsm_greeks share code but are separate entry points.
    EXPECT_DOUBLE_EQ(bsm_price(c.type, c.in), g.price);
  }
}

// A put struck at 10% of spot is worth ~1e-118. A naive N(x) = 0.5(1 + erf(...))
// returns exactly 0 here; erfc keeps full relative precision.
TEST(BsmReference, DeepOutOfTheMoneyKeepsRelativePrecision) {
  const BsmInputs in{100.0, 10.0, 0.25, 0.01, 0.0, 0.20};
  const double expected = 9.8316395518625121e-119;
  EXPECT_NEAR(bsm_price(kPut, in) / expected, 1.0, 1e-9);
}

// ---------------------------------------------------------------------------
// 3. Model-free identities over a grid of inputs
// ---------------------------------------------------------------------------

std::vector<BsmInputs> input_grid() {
  std::vector<BsmInputs> grid;
  for (double spot : {50.0, 90.0, 100.0, 110.0, 200.0}) {
    for (double T : {1.0 / 365.0, 0.1, 1.0, 5.0}) {
      for (double r : {-0.01, 0.0, 0.05}) {
        for (double q : {0.0, 0.03}) {
          for (double sigma : {0.05, 0.2, 0.8}) {
            grid.push_back({spot, 100.0, T, r, q, sigma});
          }
        }
      }
    }
  }
  return grid;
}

// C - P = S e^{-qT} - K e^{-rT}. Holds for any model, so a violation is a bug.
TEST(BsmIdentities, PutCallParity) {
  for (const auto& in : input_grid()) {
    const double lhs = bsm_price(kCall, in) - bsm_price(kPut, in);
    const double rhs = in.spot * std::exp(-in.q * in.T) - in.strike * std::exp(-in.r * in.T);
    EXPECT_TRUE(Close(lhs, rhs, 1e-12))
        << "S=" << in.spot << " T=" << in.T << " sigma=" << in.sigma;
  }
}

// Put-call parity differentiated: delta_C - delta_P = e^{-qT}, gamma and vega equal.
TEST(BsmIdentities, GreeksParity) {
  for (const auto& in : input_grid()) {
    const Greeks c = bsm_greeks(kCall, in);
    const Greeks p = bsm_greeks(kPut, in);
    EXPECT_TRUE(Close(c.delta - p.delta, std::exp(-in.q * in.T), 1e-12));
    EXPECT_TRUE(Close(c.gamma, p.gamma, 1e-12));
    EXPECT_TRUE(Close(c.vega, p.vega, 1e-12));
    EXPECT_TRUE(Close(c.rho - p.rho, in.T * in.strike * std::exp(-in.r * in.T), 1e-12));
  }
}

// No-arbitrage bounds for European options:
//   max(S e^{-qT} - K e^{-rT}, 0) <= C <= S e^{-qT}
//   max(K e^{-rT} - S e^{-qT}, 0) <= P <= K e^{-rT}
TEST(BsmIdentities, NoArbitrageBounds) {
  for (const auto& in : input_grid()) {
    const double s_fwd = in.spot * std::exp(-in.q * in.T);
    const double k_pv = in.strike * std::exp(-in.r * in.T);
    const double c = bsm_price(kCall, in);
    const double p = bsm_price(kPut, in);
    const double slack = 1e-12 * std::fmax(s_fwd, k_pv);  // rounding allowance
    EXPECT_GE(c, std::fmax(s_fwd - k_pv, 0.0) - slack);
    EXPECT_LE(c, s_fwd + slack);
    EXPECT_GE(p, std::fmax(k_pv - s_fwd, 0.0) - slack);
    EXPECT_LE(p, k_pv + slack);
  }
}

TEST(BsmIdentities, CallDecreasesAndPutIncreasesInStrike) {
  BsmInputs in{100.0, 0.0, 0.5, 0.03, 0.01, 0.25};
  double prev_call = INFINITY;
  double prev_put = -INFINITY;
  for (double k = 50.0; k <= 150.0; k += 5.0) {
    in.strike = k;
    const double c = bsm_price(kCall, in);
    const double p = bsm_price(kPut, in);
    EXPECT_LT(c, prev_call);
    EXPECT_GT(p, prev_put);
    prev_call = c;
    prev_put = p;
  }
}

TEST(BsmIdentities, PricesIncreaseInVolatility) {
  BsmInputs in{100.0, 105.0, 0.5, 0.03, 0.01, 0.0};
  double prev_call = -1.0;
  double prev_put = -1.0;
  for (double sigma = 0.05; sigma <= 1.0; sigma += 0.05) {
    in.sigma = sigma;
    const double c = bsm_price(kCall, in);
    const double p = bsm_price(kPut, in);
    EXPECT_GT(c, prev_call);
    EXPECT_GT(p, prev_put);
    prev_call = c;
    prev_put = p;
  }
}

// With q = 0, a European call is worth more the longer it has to run. (Not true
// for puts, or for calls with a large dividend yield: theta can be positive.)
TEST(BsmIdentities, CallWithoutDividendsIncreasesInMaturity) {
  BsmInputs in{100.0, 100.0, 0.0, 0.03, 0.0, 0.25};
  double prev = -1.0;
  for (double T = 0.05; T <= 5.0; T += 0.05) {
    in.T = T;
    const double c = bsm_price(kCall, in);
    EXPECT_GT(c, prev) << "T=" << T;
    prev = c;
  }
}

TEST(BsmIdentities, GreekSigns) {
  for (const auto& in : input_grid()) {
    const Greeks c = bsm_greeks(kCall, in);
    const Greeks p = bsm_greeks(kPut, in);
    EXPECT_GE(c.delta, 0.0);
    EXPECT_LE(p.delta, 0.0);
    EXPECT_GE(c.gamma, 0.0);
    EXPECT_GE(c.vega, 0.0);
    EXPECT_GE(c.rho, 0.0);
    EXPECT_LE(p.rho, 0.0);
  }
}

// ---------------------------------------------------------------------------
// 4. Analytic Greeks vs central finite differences
// ---------------------------------------------------------------------------

// Central difference (f(x+h) - f(x-h)) / 2h has error O(h^2); with h ~ 1e-4
// relative, truncation (~1e-8) and rounding (~1e-16 / 1e-4 = 1e-12) errors are
// both small, so agreement to ~1e-6 is a meaningful check.
TEST(BsmFiniteDifference, AnalyticGreeksMatchBumpAndReprice) {
  for (const auto& base : input_grid()) {
    if (base.T < 0.05) continue;  // near expiry the curvature is extreme; tested below
    for (OptionType type : {kCall, kPut}) {
      const Greeks g = bsm_greeks(type, base);
      auto price_with = [&](auto modify) {  // generic lambda: `modify` can be any callable
        BsmInputs in = base;                // copy, so base is never changed
        modify(in);
        return bsm_price(type, in);
      };

      const double hs = 1e-4 * base.spot;
      const double up = price_with([&](BsmInputs& in) { in.spot += hs; });
      const double dn = price_with([&](BsmInputs& in) { in.spot -= hs; });
      const double mid = bsm_price(type, base);
      const double fd_delta = (up - dn) / (2 * hs);
      const double fd_gamma = (up - 2 * mid + dn) / (hs * hs);

      const double hv = 1e-5;
      const double fd_vega = (price_with([&](BsmInputs& in) { in.sigma += hv; }) -
                              price_with([&](BsmInputs& in) { in.sigma -= hv; })) /
                             (2 * hv);

      const double ht = 1e-6;
      const double fd_theta = -(price_with([&](BsmInputs& in) { in.T += ht; }) -
                                price_with([&](BsmInputs& in) { in.T -= ht; })) /
                              (2 * ht);

      const double hr = 1e-6;
      const double fd_rho = (price_with([&](BsmInputs& in) { in.r += hr; }) -
                             price_with([&](BsmInputs& in) { in.r -= hr; })) /
                            (2 * hr);

      SCOPED_TRACE(::testing::Message()
                   << (type == kCall ? "call" : "put") << " S=" << base.spot << " T=" << base.T
                   << " r=" << base.r << " q=" << base.q << " sigma=" << base.sigma);
      EXPECT_TRUE(Close(g.delta, fd_delta, 1e-6));
      EXPECT_TRUE(Close(g.vega, fd_vega, 1e-6));
      EXPECT_TRUE(Close(g.theta, fd_theta, 1e-5));
      EXPECT_TRUE(Close(g.rho, fd_rho, 1e-5));
      // The second difference divides rounding error by h^2, so it is noisier.
      EXPECT_TRUE(Close(g.gamma, fd_gamma, 1e-4));
    }
  }
}

// ---------------------------------------------------------------------------
// Edge cases
// ---------------------------------------------------------------------------

TEST(BsmEdgeCases, AtExpiryPriceIsIntrinsic) {
  for (double spot : {80.0, 100.0, 120.0}) {
    const BsmInputs in{spot, 100.0, 0.0, 0.05, 0.02, 0.3};
    EXPECT_DOUBLE_EQ(bsm_price(kCall, in), std::fmax(spot - 100.0, 0.0));
    EXPECT_DOUBLE_EQ(bsm_price(kPut, in), std::fmax(100.0 - spot, 0.0));
  }
}

TEST(BsmEdgeCases, ZeroVolIsDiscountedForwardPayoff) {
  const BsmInputs in{100.0, 100.0, 1.0, 0.05, 0.0, 0.0};
  // Forward = 100 e^{0.05} > K, so the call pays off for sure: S - K e^{-rT}.
  // Not EXPECT_DOUBLE_EQ: the compiler may evaluate std::exp(-0.05) here at
  // compile time and round it differently from the library's runtime call
  // (Clang does). Floating-point results should be compared with a tolerance.
  EXPECT_TRUE(Close(bsm_price(kCall, in), 100.0 - 100.0 * std::exp(-0.05), 1e-14));
  EXPECT_DOUBLE_EQ(bsm_price(kPut, in), 0.0);

  const Greeks g = bsm_greeks(kCall, in);
  EXPECT_DOUBLE_EQ(g.delta, 1.0);
  EXPECT_DOUBLE_EQ(g.gamma, 0.0);
  EXPECT_DOUBLE_EQ(g.vega, 0.0);
  EXPECT_TRUE(Close(g.rho, 100.0 * std::exp(-0.05), 1e-15));
}

// The formula should approach the zero-vol limit smoothly, not jump.
TEST(BsmEdgeCases, TinyVolApproachesZeroVolLimit) {
  const BsmInputs zero{100.0, 90.0, 1.0, 0.05, 0.01, 0.0};
  BsmInputs tiny = zero;
  tiny.sigma = 1e-8;
  EXPECT_TRUE(Close(bsm_price(kCall, tiny), bsm_price(kCall, zero), 1e-12));
  EXPECT_TRUE(Close(bsm_price(kPut, tiny), bsm_price(kPut, zero), 1e-12));
}

TEST(BsmEdgeCases, NearExpiryAtTheMoneyIsFinite) {
  const BsmInputs in{100.0, 100.0, 1e-10, 0.05, 0.0, 0.2};
  const Greeks g = bsm_greeks(kCall, in);
  EXPECT_TRUE(std::isfinite(g.price));
  EXPECT_TRUE(std::isfinite(g.gamma));  // large (~4e5) but finite
  EXPECT_TRUE(std::isfinite(g.theta));
  EXPECT_NEAR(g.delta, 0.5, 1e-4);
}

TEST(BsmEdgeCases, InvalidInputsThrow) {
  const BsmInputs good{100.0, 100.0, 1.0, 0.05, 0.0, 0.2};
  auto with = [&](auto modify) {
    BsmInputs in = good;
    modify(in);
    return in;
  };
  const std::vector<BsmInputs> bad{
      with([](BsmInputs& in) { in.spot = 0.0; }),   with([](BsmInputs& in) { in.spot = -1.0; }),
      with([](BsmInputs& in) { in.strike = 0.0; }), with([](BsmInputs& in) { in.T = -0.1; }),
      with([](BsmInputs& in) { in.sigma = -0.2; }), with([](BsmInputs& in) { in.sigma = NAN; }),
      with([](BsmInputs& in) { in.spot = NAN; }),   with([](BsmInputs& in) { in.r = INFINITY; }),
      with([](BsmInputs& in) { in.T = INFINITY; }),
  };
  for (const auto& in : bad) {
    EXPECT_THROW((void)bsm_price(kCall, in), std::invalid_argument);
    EXPECT_THROW((void)bsm_greeks(kPut, in), std::invalid_argument);
  }
}

}  // namespace
