// pybind11 bindings: exposes the C++ core to Python as `optlib._core`.
//
// Keep this file thin. It should only translate between Python and C++
// (argument names, docstrings, NumPy arrays); all logic lives in src/.

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <array>
#include <string>

#include "optlib/black_scholes.hpp"
#include "optlib/rates.hpp"
#include "optlib/version.hpp"

namespace py = pybind11;
using namespace py::literals;  // enables the "name"_a syntax for keyword arguments

namespace {

// Scalar adapters with one argument per input. py::vectorize (below) needs a
// function of plain scalars to broadcast over NumPy arrays; it can't see inside
// the BsmInputs struct.
double price_scalar(optlib::OptionType type, double spot, double strike, double T, double r,
                    double sigma, double q) {
  return optlib::bsm_price(type, {spot, strike, T, r, q, sigma});
}

// Vectorized Greeks. py::vectorize only handles functions returning a single
// number, so this loops by hand: broadcast all inputs to a common shape with
// NumPy's own rules, then fill six output arrays in one pass so d1, d2 and
// N(.) are computed once per option rather than once per Greek.
py::dict greeks_array(optlib::OptionType type, const py::array_t<double>& spot,
                      const py::array_t<double>& strike, const py::array_t<double>& T,
                      const py::array_t<double>& r, const py::array_t<double>& sigma,
                      const py::array_t<double>& q) {
  // np.broadcast_arrays applies standard NumPy broadcasting (scalars, (n,1) x (1,m), ...).
  py::module_ np = py::module_::import("numpy");
  py::tuple b = np.attr("broadcast_arrays")(spot, strike, T, r, sigma, q);
  // forcecast + c_style: make each a C-contiguous float64 array (copying if needed).
  using Arr = py::array_t<double, py::array::c_style | py::array::forcecast>;
  std::array<Arr, 6> in{Arr::ensure(b[0]), Arr::ensure(b[1]), Arr::ensure(b[2]),
                        Arr::ensure(b[3]), Arr::ensure(b[4]), Arr::ensure(b[5])};

  const auto shape = std::vector<py::ssize_t>(in[0].shape(), in[0].shape() + in[0].ndim());
  std::array<py::array_t<double>, 6> out{py::array_t<double>(shape), py::array_t<double>(shape),
                                         py::array_t<double>(shape), py::array_t<double>(shape),
                                         py::array_t<double>(shape), py::array_t<double>(shape)};

  // Raw pointers into the arrays' memory. `const double*` for inputs: we
  // promise not to write through them.
  const double* s = in[0].data();
  const double* k = in[1].data();
  const double* t = in[2].data();
  const double* rr = in[3].data();
  const double* v = in[4].data();
  const double* qq = in[5].data();
  double* p_price = out[0].mutable_data();
  double* p_delta = out[1].mutable_data();
  double* p_gamma = out[2].mutable_data();
  double* p_vega = out[3].mutable_data();
  double* p_theta = out[4].mutable_data();
  double* p_rho = out[5].mutable_data();

  const py::ssize_t n = in[0].size();
  for (py::ssize_t i = 0; i < n; ++i) {
    const optlib::Greeks g = optlib::bsm_greeks(type, {s[i], k[i], t[i], rr[i], qq[i], v[i]});
    p_price[i] = g.price;
    p_delta[i] = g.delta;
    p_gamma[i] = g.gamma;
    p_vega[i] = g.vega;
    p_theta[i] = g.theta;
    p_rho[i] = g.rho;
  }
  return py::dict("price"_a = out[0], "delta"_a = out[1], "gamma"_a = out[2], "vega"_a = out[3],
                  "theta"_a = out[4], "rho"_a = out[5]);
}

}  // namespace

// The module name here must match the target name in CMakeLists.txt (_core).
PYBIND11_MODULE(_core, m) {
  m.doc() = "C++ core of optlib. Import from `optlib`, not from this module directly.";

  // std::string_view isn't owned memory, so copy to std::string for Python.
  m.def(
      "version", [] { return std::string(optlib::version()); },
      "Version of the compiled C++ core.");

  m.def("discount_factor", &optlib::discount_factor, "r"_a, "T"_a,
        R"doc(
Continuously compounded discount factor exp(-r * T).

Parameters
----------
r : float
    Continuously compounded risk-free rate (0.05 = 5%).
T : float
    Time to maturity in years.
)doc");

  // C++ enum class -> Python enum-like type: optlib.OptionType.Call / .Put.
  py::enum_<optlib::OptionType>(m, "OptionType")
      .value("Call", optlib::OptionType::Call)
      .value("Put", optlib::OptionType::Put);

  // Exceptions: pybind11 translates std::invalid_argument to Python ValueError
  // automatically, so invalid inputs raise ValueError in Python.

  // py::vectorize wraps a scalar function so it accepts NumPy arrays and
  // broadcasts them, looping in C++. The OptionType argument is an enum, which
  // py::vectorize passes through unchanged rather than broadcasting.
  m.def("bsm_price", py::vectorize(price_scalar), "option_type"_a, "spot"_a, "strike"_a, "T"_a,
        "r"_a, "sigma"_a, "q"_a = 0.0,
        "Black-Scholes-Merton price. Array inputs broadcast like NumPy.");

  m.def("bsm_greeks", &greeks_array, "option_type"_a, "spot"_a, "strike"_a, "T"_a, "r"_a, "sigma"_a,
        "q"_a = 0.0,
        "Black-Scholes-Merton price and Greeks as a dict of arrays. Inputs broadcast like NumPy.");
}
