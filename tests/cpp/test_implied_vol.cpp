// Tests for implied_vol(): input validation and no-arbitrage bound handling.
// (Solver accuracy tests are added in piece 4.)

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

#include "optlib/black_scholes.hpp"
#include "optlib/implied_vol.hpp"

namespace {
using optlib::bsm_price;
using optlib::BsmInputs;
using optlib::implied_vol;
using optlib::ImpliedVolOptions;
using optlib::ImpliedVolResult;
using optlib::IvStatus;
using optlib::OptionType;

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

constexpr auto kCall = OptionType::Call;
constexpr auto kPut = OptionType::Put;

constexpr double kS = 100.0, kT = 1.0, kR = 0.05, kQ = 0.02;

// Test throws on spot 0 or spot -1
TEST(ImpliedVolValidation, ThrowsOnNonPositiveSpot) {
  EXPECT_THROW((void)implied_vol(kCall, 5.0, 0.0, 100.0, kT, kR, kQ), std::invalid_argument);
  EXPECT_THROW((void)implied_vol(kCall, 5.0, -1.0, 100.0, kT, kR, kQ), std::invalid_argument);
}
// Test throws on strike 0 or -1
TEST(ImpliedVolValidation, ThrowsOnNonPositiveStrike) {
  EXPECT_THROW((void)implied_vol(kCall, 5.0, kS, 0.0, kT, kR, kQ), std::invalid_argument);
  EXPECT_THROW((void)implied_vol(kCall, 5.0, kS, -1.0, kT, kR, kQ), std::invalid_argument);
}
// Test throws on T 0
TEST(ImpliedVolValidation, ThrowsOnNonPositiveT) {
  EXPECT_THROW((void)implied_vol(kCall, 5.0, kS, 100.0, 0.0, kR, kQ), std::invalid_argument);
}
// Test throws on NaN price or infinite r
TEST(ImpliedVolValidation, ThrowsOnNonFiniteInputs) {
  EXPECT_THROW(
      (void)implied_vol(kCall, std::numeric_limits<double>::quiet_NaN(), kS, 100.0, kT, kR, kQ),
      std::invalid_argument);
  EXPECT_THROW(
      (void)implied_vol(kCall, 5.0, kS, 100.0, kT, std::numeric_limits<double>::infinity(), kQ),
      std::invalid_argument);
}

TEST(ImpliedVolBounds, ItmCallAtIntrinsicReturnsZeroSigma) {
  // With sigma = 0 the BSM price is exactly at the lower bound.
  const double K = 100.0;
  const double price = bsm_price(kCall, BsmInputs{kS, K, kT, kR, 0.0, kQ});
  const ImpliedVolResult res = implied_vol(kCall, price, kS, K, kT, kR, kQ);
  EXPECT_EQ(res.status, IvStatus::PriceAtLowerBound);
  EXPECT_EQ(res.iterations, 0);
  EXPECT_EQ(res.sigma, 0.0);
}

TEST(ImpliedVolBounds, NegativePriceIsStatusNotThrow) {
  const double K = 100.0;
  const double price = -1.0;
  const ImpliedVolResult res = implied_vol(kCall, price, kS, K, kT, kR, kQ);
  EXPECT_EQ(res.status, IvStatus::PriceBelowLowerBound);
  EXPECT_TRUE(std::isnan(res.sigma));
}

TEST(ImpliedVolBounds, PriceBelowIntrinsic) {
  const double K = 100.0;
  const double intrinsic = kS * std::exp(-kQ * kT) - K * std::exp(-kR * kT);  // about 21.9
  const ImpliedVolResult res = implied_vol(kCall, intrinsic - 1.0, kS, K, kT, kR, kQ);
  EXPECT_EQ(res.status, IvStatus::PriceBelowLowerBound);
  EXPECT_TRUE(std::isnan(res.sigma));
}

TEST(ImpliedVolBounds, OtmPutAtZeroReturnsZeroSigma) {
  const double K = 100.0;
  const double price = 0.0;
  const ImpliedVolResult res = implied_vol(kPut, price, kS, K, kT, kR, kQ);
  EXPECT_EQ(res.status, IvStatus::PriceAtLowerBound);
  EXPECT_EQ(res.iterations, 0);
  EXPECT_EQ(res.sigma, 0.0);
}

TEST(ImpliedVolBounds, CallAtUpperBound) {
  const double K = 100.0;
  const double price = kS * std::exp(-kQ * kT);
  const ImpliedVolResult res = implied_vol(kCall, price, kS, K, kT, kR, kQ);
  EXPECT_EQ(res.status, IvStatus::PriceAtOrAboveUpperBound);
}

TEST(ImpliedVolBounds, PutAboveUpperBound) {
  const double K = 100.0;
  const double price = K * std::exp(-kR * kT) + 1.0;
  const ImpliedVolResult res = implied_vol(kPut, price, kS, K, kT, kR, kQ);
  EXPECT_EQ(res.status, IvStatus::PriceAtOrAboveUpperBound);
}

TEST(ImpliedVolSolver, RoundTripGrid) {
  std::vector<OptionType> types = {OptionType::Call, OptionType::Put};
  std::vector<double> Ks = {50.0, 80.0, 100.0, 120.0, 200.0};
  std::vector<double> Ts = {0.05, 0.25, 1.0, 5.0};
  std::vector<double> sigmas = {0.05, 0.2, 0.5, 1.0, 2.0};

  for (OptionType type : types) {
    for (double K : Ks) {
      for (double T : Ts) {
        for (double sigma : sigmas) {
          SCOPED_TRACE(testing::Message()
                       << "Iteration type = " << (type == kCall ? "call" : "put") << ", K = " << K
                       << ", T = " << T << ", sigma = " << sigma);
          const double price = bsm_price(type, BsmInputs{kS, K, T, kR, sigma, kQ});
          const double Sd = kS * std::exp(-kQ * T);
          const double Kd = K * std::exp(-kR * T);
          const double lower = std::max(optlib::sign(type) * (Sd - Kd), 0.0);
          const double time_value = price - lower;
          if (time_value < 1e-12) continue;
          const ImpliedVolResult res = implied_vol(type, price, kS, K, T, kR, kQ);
          EXPECT_EQ(res.status, IvStatus::Converged);

          // Deep ITM prices are mostly intrinsic value: a double carries too few digits
          // of time value to pin down sigma, so check sigma itself only when the time
          // value is a meaningful share of the price. The reprice check always applies.
          if (time_value / price > 1e-8) {
            EXPECT_NEAR(res.sigma, sigma, 1e-8);
          }
          EXPECT_TRUE(Close(bsm_price(type, BsmInputs{kS, K, T, kR, res.sigma, kQ}), price, 1e-12));
          // Contracts with real time value converge in a handful of Newton steps (at most
          // 13 on this grid). Far-wing contracts spend many iterations bisecting, and the
          // exact count depends on last-digit rounding, which differs between GCC and
          // Clang (32 vs 60 for the K = 50, T = 0.25 put). For those, only convergence,
          // checked above, is required.
          if (time_value > 1e-4) {
            EXPECT_LE(res.iterations, 20);
          }
        }
      }
    }
  }
}

TEST(ImpliedVolSolver, RoundTripAtTheMoney) {
  const double K = 100.0;
  const double price = bsm_price(kCall, BsmInputs{kS, K, kT, kR, 0.20, kQ});
  const ImpliedVolResult res = implied_vol(OptionType::Call, price, kS, K, kT, kR, kQ);
  EXPECT_EQ(res.status, IvStatus::Converged);
  EXPECT_LE(res.iterations, 10);
  EXPECT_NEAR(res.sigma, 0.20, 1e-12);
  EXPECT_TRUE(Close(bsm_price(kCall, BsmInputs{kS, K, kT, kR, res.sigma, kQ}), price, 1e-12));
}

TEST(ImpliedVolBounds, SigmaAboveMaxReported) {
  const double K = 100.0;
  const double price = bsm_price(kCall, BsmInputs{kS, K, kT, kR, 3.0, kQ});
  ImpliedVolOptions opts;
  opts.sigma_max = 2.0;
  const ImpliedVolResult res = implied_vol(kCall, price, kS, K, kT, kR, kQ, opts);
  EXPECT_EQ(res.status, IvStatus::SigmaAboveMax);
  EXPECT_TRUE(std::isnan(res.sigma));
}

TEST(ImpliedVolBounds, SigmaAtMaxIsNotAboveMax) {
  const double K = 100.0;
  const double price = bsm_price(kCall, BsmInputs{kS, K, kT, kR, 2.0, kQ});
  ImpliedVolOptions opts;
  opts.sigma_max = 2.0;
  const ImpliedVolResult res = implied_vol(kCall, price, kS, K, kT, kR, kQ, opts);
  EXPECT_EQ(res.status, IvStatus::Converged);
}

TEST(ImpliedVolBounds, MaxIterationsExceededReported) {
  const double K = 150.0;
  const double price = bsm_price(kCall, BsmInputs{kS, K, kT, kR, 2.0, kQ});
  ImpliedVolOptions opts;
  opts.max_iter = 1;
  const ImpliedVolResult res = implied_vol(kCall, price, kS, K, kT, kR, kQ, opts);
  EXPECT_EQ(res.status, IvStatus::MaxIterationsExceeded);
  EXPECT_TRUE(std::isnan(res.sigma));
  EXPECT_EQ(res.iterations, 1);
}

}  // namespace
