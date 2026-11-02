// pybind11 bindings: exposes the C++ core to Python as `optlib._core`.
//
// Keep this file thin. It should only translate between Python and C++
// (argument names, docstrings, NumPy arrays); all logic lives in src/.

#include <pybind11/pybind11.h>

#include <string>

#include "optlib/rates.hpp"
#include "optlib/version.hpp"

namespace py = pybind11;
using namespace py::literals;  // enables the "name"_a syntax for keyword arguments

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
}
