# optlib

[![CI](https://github.com/beecherbrad-pixel/options-pricing/actions/workflows/ci.yml/badge.svg)](https://github.com/beecherbrad-pixel/options-pricing/actions/workflows/ci.yml)

An options pricing library with a C++17 core and a Python API (pybind11).

> **Status: early development (v0.0.1).** Black–Scholes–Merton pricing and Greeks are implemented
> and tested. Implied volatility, trees, Monte Carlo and the volatility surface are still to come;
> see the [roadmap](#roadmap).

## Quickstart

Requires Python 3.11+, CMake 3.20+ and a C++17 compiler (GCC 9+, Clang 10+ or MSVC 2019+).

```bash
pip install .
```

```python
import numpy as np
import optlib

# Hull's textbook example: S=42, K=40, T=0.5y, r=10%, sigma=20%  ->  4.7594
optlib.bsm_price("call", spot=42, strike=40, T=0.5, r=0.10, sigma=0.20)

# Inputs broadcast like NumPy: a 3 x 41 grid of expiries x strikes in one C++ call
strikes = np.linspace(80, 120, 41)
expiries = np.array([[0.25], [0.5], [1.0]])
prices = optlib.bsm_price("put", 100, strikes, expiries, r=0.03, sigma=0.25, q=0.01)

# Price and all five Greeks at once
optlib.bsm_greeks("call", 49, 50, 20 / 52, 0.05, 0.20)
# {'price': 2.4005, 'delta': 0.5216, 'gamma': 0.0655, 'vega': 12.1055, 'theta': -4.3053, 'rho': 8.907}
```

## Features

| Component | Status | Source |
|---|---|---|
| Black–Scholes–Merton price, continuous dividend yield | done | [`black_scholes.hpp`](include/optlib/black_scholes.hpp) |
| Analytic Greeks: delta, gamma, vega, theta, rho | done | [`black_scholes.cpp`](src/black_scholes.cpp) |
| Vectorised NumPy bindings with broadcasting | done | [`module.cpp`](bindings/module.cpp) |
| Implied volatility solver | planned | |
| CRR binomial tree, American exercise | planned | |
| Monte Carlo with variance reduction | planned | |
| SVI volatility surface on Deribit data | planned | |

**Units.** Time in years, rates and vols as decimals (0.05, 0.20). Greeks are per 1.00 change in
the input and per year: divide vega by 100 for "per vol point", theta by 365 for "per day", and
rho by 10,000 for "per basis point".

## Validation

Tests run in CI on GCC and Clang (warnings as errors) and Python 3.11 and 3.13.

- **Textbook values:** Hull's worked examples for prices and all five Greeks, at Hull's printed precision.
- **High-precision references:** prices and Greeks agree with 30-digit `mpmath` values to 1e-12.
- **Independent implementation:** 2,000 random contracts agree with a NumPy/SciPy implementation
  to 1e-10.
- **Put–call parity** (prices and Greeks) to 1e-12 across a 360-point grid including negative
  rates and dividend yields.
- **Greeks vs finite differences:** each analytic Greek matches central bump-and-reprice.
- **No-arbitrage bounds and monotonicity:** prices lie within model-free bounds, calls fall and
  puts rise with strike, prices rise with vol.
- **Edge cases:** intrinsic value at expiry, the zero-vol limit, deep out-of-the-money prices
  (~1e-118) to full relative precision, and invalid inputs raise errors instead of returning
  garbage.

## Development

On Windows, develop inside **WSL2** (Ubuntu): CI runs on Linux, and the toolchain is simpler there.

```bash
# One-time setup (Ubuntu / WSL2)
sudo apt install build-essential cmake ninja-build python3-venv python3-dev git clang-format
python3 -m venv .venv && source .venv/bin/activate

# C++: build and run the GoogleTest suite
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure

# Python: build the extension, run pytest and the linter
pip install ".[dev]"      # rerun after changing any C++ file
pytest
ruff check . && ruff format --check .
```

## Layout

```
include/optlib/   public C++ headers
src/              C++ implementations
bindings/         pybind11 module (thin translation layer only)
python/optlib/    Python package (re-exports the C++ core)
tests/cpp/        GoogleTest
tests/python/     pytest
```

## Roadmap

- [x] Phase 0: CMake + pybind11 + scikit-build-core skeleton, tests, CI
- [x] Phase 1: Black–Scholes–Merton prices and analytic Greeks
- [ ] Phase 2: Safeguarded implied-volatility solver
- [ ] Phase 3: CRR binomial tree (European and American)
- [ ] Phase 4: Monte Carlo with variance reduction
- [ ] Phase 5: SVI volatility surface on live Deribit option chains, with arbitrage checks
- [ ] Phase 6: Benchmarks, documentation, v0.1 release

## License

MIT. See [LICENSE](LICENSE).
