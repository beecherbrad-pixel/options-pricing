// Tests for implied_vol(): input validation and no-arbitrage bound handling.
// (Solver accuracy tests are added in piece 4.)

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <stdexcept>

#include "optlib/black_scholes.hpp"
#include "optlib/implied_vol.hpp"

namespace {
using optlib::bsm_price;
using optlib::BsmInputs;
using optlib::implied_vol;
using optlib::ImpliedVolResult;
using optlib::ImpliedVolOptions;
using optlib::IvStatus;
using optlib::OptionType;

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

TEST(ImpliedVolBounds, InteriorPriceIsStubbedForNow) {
  // This is a placeholder that will be replaced
  const double K = 100.0;
  const double sigma = 0.20;
  const double price = bsm_price(kCall, BsmInputs{kS, K, kT, kR, sigma, kQ});
  const ImpliedVolResult res = implied_vol(kCall, price, kS, K, kT, kR, kQ);
  EXPECT_EQ(res.status, IvStatus::MaxIterationsExceeded);
  EXPECT_TRUE(std::isnan(res.sigma));
}

TEST(ImpliedVolBounds, SigmaAboveMaxReported) {
  const double K = 100.0;
  const double price = bsm_price(kCall, BsmInputs{kS, K, kT, kR, 3.0, kQ});
  const ImpliedVolResult res = implied_vol(kCall, price, kS, K, kT, kR, kQ, {1e-12, 100, 2.0});
  EXPECT_EQ(res.status, IvStatus::SigmaAboveMax);
  EXPECT_TRUE(std::isnan(res.sigma));
}

TEST(ImpliedVolBounds, SigmaAtMaxIsNotAboveMax) {
  const double K = 100.0;
  const double price = bsm_price(kCall, BsmInputs{kS, K, kT, kR, 2.0, kQ});
  ImpliedVolOptions opts;
  opts.sigma_max = 2.0;
  const ImpliedVolResult res = implied_vol(kCall, price, kS, K, kT, kR, kQ, opts);
  EXPECT_EQ(res.status, IvStatus::MaxIterationsExceeded);
}
}  // namespace
