# optlib

[![CI](https://github.com/beecherbrad-pixel/options-pricing/actions/workflows/ci.yml/badge.svg)](https://github.com/beecherbrad-pixel/options-pricing/actions/workflows/ci.yml)

An options pricing library with a C++17 core and a Python API (pybind11).

> **Status: early development (v0.0.1).** Only the build, packaging and CI scaffolding exist so far.
> No pricing models are implemented yet. See the [roadmap](#roadmap).

## Quickstart

Requires Python 3.11+, CMake 3.20+ and a C++17 compiler (GCC 9+, Clang 10+ or MSVC 2019+).

```bash
pip install .
python -c "import optlib; print(optlib.__version__, optlib.discount_factor(r=0.05, T=1.0))"
```

## Development

On Windows, develop inside **WSL2** (Ubuntu): CI runs on Linux, and the toolchain is simpler there.

```bash
# One-time setup (Ubuntu / WSL2)
sudo apt install build-essential cmake ninja-build python3-venv
python3 -m venv .venv && source .venv/bin/activate

# C++: build and run the GoogleTest suite
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure

# Python: build the extension, run pytest and the linter
pip install -e ".[dev]"   # or: pip install ".[dev]"
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
- [ ] Phase 1: Black–Scholes–Merton prices and analytic Greeks
- [ ] Phase 2: Safeguarded implied-volatility solver
- [ ] Phase 3: CRR binomial tree (European and American)
- [ ] Phase 4: Monte Carlo with variance reduction
- [ ] Phase 5: SVI volatility surface on live Deribit option chains, with arbitrage checks
- [ ] Phase 6: Benchmarks, documentation, v0.1 release

## License

MIT. See [LICENSE](LICENSE).
