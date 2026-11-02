#include "optlib/rates.hpp"

#include <cmath>

namespace optlib {

double discount_factor(double r, double T) noexcept { return std::exp(-r * T); }

}  // namespace optlib
