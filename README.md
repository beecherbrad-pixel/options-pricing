# optlib

[![CI](https://github.com/beecherbrad-pixel/options-pricing/actions/workflows/ci.yml/badge.svg)](https://github.com/beecherbrad-pixel/options-pricing/actions/workflows/ci.yml)

An options pricing library with a C++17 core and a Python API (pybind11).

> **Status: early development (v0.0.1).** Black–Scholes–Merton pricing, Greeks and implied
> volatility are implemented and tested. Trees, Monte Carlo and the volatility surface are still to
> come; see the [roadmap](#roadmap).

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

# Implied volatility: recover sigma from a price (vectorised the same way)
price = optlib.bsm_price("call", 100, 100, 1.0, 0.05, sigma=0.20)
optlib.implied_vol("call", price, 100, 100, 1.0, 0.05)  # 0.2 (to ~1e-15)

# Prices with no implied vol give NaN; full_output says why
optlib.implied_vol("call", 1.0, 100, 50, 1.0, 0.05, full_output=True)
# {'sigma': nan, 'iterations': 0, 'status': 2}
# status 2 == int(optlib.IvStatus.PriceBelowLowerBound): the price is below intrinsic value
```

## Features

| Component | Status | Source |
|---|---|---|
| Black–Scholes–Merton price, continuous dividend yield | done | [`black_scholes.hpp`](include/optlib/black_scholes.hpp) |
| Analytic Greeks: delta, gamma, vega, theta, rho | done | [`black_scholes.cpp`](src/black_scholes.cpp) |
| Vectorised NumPy bindings with broadcasting | done | [`module.cpp`](bindings/module.cpp) |
| Implied volatility: safeguarded Newton, no-arbitrage checks | done | [`implied_vol.hpp`](include/optlib/implied_vol.hpp) |
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

Implied volatility:

- **Round trips:** a 200-contract C++ grid (calls and puts; strikes 50–200; expiries 0.05–5 years;
  vols 5–200%) and 2,000 random contracts per option type in Python. Every contract converges and
  reprices to 1e-10 or better; sigma is recovered to 1e-8 wherever the price carries enough time
  value to determine it (see [known limits](#implied-volatility)).
- **Independent solver:** agrees with SciPy's `brentq` to 1e-8.
- **Every status and boundary:** prices exactly at each no-arbitrage bound, negative prices, sigma
  exactly at the search limit, and iteration limits. Boundary comparisons allow for the last-digit
  rounding that put–call parity introduces: without that tolerance, about 16% of prices priced at
  exactly the search limit were misreported in testing.

## Implied volatility

`implied_vol` finds the volatility at which the BSM price matches an observed price. Price is
strictly increasing in volatility, so a price inside the no-arbitrage bounds has exactly one
implied vol, and the solver is built around that fact:

1. **No-arbitrage check.** A price below the discounted intrinsic value, or at or above the upper
   bound (`S e^{-qT}` for a call, `K e^{-rT}` for a put), has no implied vol. These return NaN with
   a status code rather than raising, because on market data they are data problems, not
   programming errors. Invalid inputs (spot, strike or T ≤ 0, or non-finite values) do raise.
2. **In-the-money to out-of-the-money.** By put–call parity an in-the-money option has the same
   implied vol as the opposite option at the same strike, whose price is exactly the time value
   `price − intrinsic`. The solver works on that price, which carries only the vol-sensitive part.
3. **Starting point and bracket.** Newton starts at the Manaster–Koehler point
   `sigma = sqrt(2 |ln(F/K)| / T)`, the inflection point of price as a function of vol, from which
   Newton converges monotonically. The root is bracketed in `[0, sigma_max]` (default 10, i.e.
   1000%).
4. **Safeguarded Newton.** A Newton step is accepted only if it stays strictly inside the bracket,
   which shrinks after every evaluation; otherwise the solver bisects. It stops when the step or the
   bracket falls below `1e-12`.

**Statuses** (`optlib.IvStatus`): `Converged`, `PriceAtLowerBound` (no time value, sigma = 0),
`PriceBelowLowerBound`, `PriceAtOrAboveUpperBound`, `SigmaAboveMax` and `MaxIterationsExceeded`.
Sigma is NaN for all but the first two.

**Known limits.**

- *Deep in-the-money precision.* Such prices are almost all intrinsic value, and a float64 price
  holds only ~16 significant digits, so too few digits of time value may survive to pin down vol.
  The result always reprices the input exactly, but sigma itself can be off by up to ~1e-5 in
  extreme cases (for example K = 50, S = 100, T = 0.25, sigma = 20%). Solving the equivalent
  out-of-the-money option directly from its own price recovers sigma to ~1e-15.
- *Iteration count.* Typical contracts converge in 4–10 iterations. Far out-of-the-money,
  short-dated contracts, where price behaves like `exp(-c / sigma^2)`, can take 20–50. Newton on
  log-price fixes those wings but is slower near the money at high vol. Jäckel's
  ["Let's Be Rational"](http://www.jaeckel.org/LetsBeRational.pdf) (2015) reaches machine
  precision in two iterations everywhere and is the natural benchmark for a later phase.

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
- [x] Phase 2: Safeguarded implied-volatility solver
- [ ] Phase 3: CRR binomial tree (European and American)
- [ ] Phase 4: Monte Carlo with variance reduction
- [ ] Phase 5: SVI volatility surface on live Deribit option chains, with arbitrage checks
- [ ] Phase 6: Benchmarks, documentation, v0.1 release

## License

MIT. See [LICENSE](LICENSE).
