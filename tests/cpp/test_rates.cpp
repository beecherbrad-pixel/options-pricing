#include <gtest/gtest.h>

#include <cmath>

#include "optlib/rates.hpp"

namespace {

TEST(DiscountFactor, ZeroRateIsOne) { EXPECT_DOUBLE_EQ(optlib::discount_factor(0.0, 5.0), 1.0); }

TEST(DiscountFactor, ZeroMaturityIsOne) {
  EXPECT_DOUBLE_EQ(optlib::discount_factor(0.05, 0.0), 1.0);
}

TEST(DiscountFactor, MatchesClosedForm) {
  // 5% for 1 year: exp(-0.05) = 0.951229424500714...
  EXPECT_NEAR(optlib::discount_factor(0.05, 1.0), 0.951229424500714, 1e-15);
}

TEST(DiscountFactor, CompoundsMultiplicatively) {
  // P(0, T1 + T2) = P(0, T1) * P(0, T2) under a flat rate.
  const double r = 0.03;
  EXPECT_NEAR(optlib::discount_factor(r, 2.5),
              optlib::discount_factor(r, 1.0) * optlib::discount_factor(r, 1.5), 1e-15);
}

TEST(DiscountFactor, NegativeRatesGiveFactorAboveOne) {
  // Negative rates happened (EUR, CHF, JPY); the formula must still behave.
  EXPECT_GT(optlib::discount_factor(-0.005, 1.0), 1.0);
}

}  // namespace
