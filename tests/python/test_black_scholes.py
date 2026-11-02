"""Black-Scholes-Merton tests from Python.

The C++ suite (tests/cpp) checks the maths in depth. These tests check the
Python API and cross-check against an independent pure-NumPy/SciPy
implementation, so a mistake would have to be made identically in two
languages to slip through.
"""

import numpy as np
import pytest
from scipy.stats import norm

import optlib
from optlib import OptionType, bsm_greeks, bsm_price


def scipy_bsm(kind, S, K, T, r, sigma, q=0.0):
    """Independent reference implementation of BSM price and Greeks."""
    w = 1.0 if kind == "call" else -1.0
    sqrt_t = np.sqrt(T)
    d1 = (np.log(S / K) + (r - q + 0.5 * sigma**2) * T) / (sigma * sqrt_t)
    d2 = d1 - sigma * sqrt_t
    df_r, df_q = np.exp(-r * T), np.exp(-q * T)
    return {
        "price": w * (S * df_q * norm.cdf(w * d1) - K * df_r * norm.cdf(w * d2)),
        "delta": w * df_q * norm.cdf(w * d1),
        "gamma": df_q * norm.pdf(d1) / (S * sigma * sqrt_t),
        "vega": S * df_q * norm.pdf(d1) * sqrt_t,
        "theta": (
            -S * df_q * norm.pdf(d1) * sigma / (2 * sqrt_t)
            - w * r * K * df_r * norm.cdf(w * d2)
            + w * q * S * df_q * norm.cdf(w * d1)
        ),
        "rho": w * K * T * df_r * norm.cdf(w * d2),
    }


@pytest.fixture(scope="module")
def random_inputs():
    """A seeded random grid covering a wide range of moneyness, maturity and vol."""
    rng = np.random.default_rng(20261102)
    n = 2_000
    return {
        "S": rng.uniform(20, 200, n),
        "K": rng.uniform(20, 200, n),
        "T": rng.uniform(0.01, 3.0, n),
        "r": rng.uniform(-0.01, 0.08, n),
        "sigma": rng.uniform(0.05, 1.0, n),
        "q": rng.uniform(0.0, 0.05, n),
    }


@pytest.mark.parametrize("kind", ["call", "put"])
def test_matches_scipy_reference(kind, random_inputs):
    x = random_inputs
    ours = bsm_greeks(kind, x["S"], x["K"], x["T"], x["r"], x["sigma"], x["q"])
    ref = scipy_bsm(kind, x["S"], x["K"], x["T"], x["r"], x["sigma"], x["q"])
    for name, expected in ref.items():
        np.testing.assert_allclose(ours[name], expected, rtol=1e-10, atol=1e-12, err_msg=name)


def test_put_call_parity_vectorized(random_inputs):
    x = random_inputs
    args = (x["S"], x["K"], x["T"], x["r"], x["sigma"], x["q"])
    lhs = bsm_price("call", *args) - bsm_price("put", *args)
    rhs = x["S"] * np.exp(-x["q"] * x["T"]) - x["K"] * np.exp(-x["r"] * x["T"])
    np.testing.assert_allclose(lhs, rhs, rtol=1e-12, atol=1e-12)


def test_hull_example():
    assert bsm_price("call", 42, 40, 0.5, 0.10, 0.20) == pytest.approx(4.76, abs=0.005)
    assert bsm_price("put", 42, 40, 0.5, 0.10, 0.20) == pytest.approx(0.81, abs=0.005)


def test_scalar_inputs_return_float():
    assert isinstance(bsm_price("call", 100, 100, 1, 0.05, 0.2), float)
    g = bsm_greeks("call", 100, 100, 1, 0.05, 0.2)
    assert all(isinstance(v, float) for v in g.values())


def test_broadcasting_strike_by_expiry_grid():
    strikes = np.linspace(80, 120, 9)
    expiries = np.array([0.25, 0.5, 1.0])
    prices = bsm_price("call", 100.0, strikes[None, :], expiries[:, None], 0.03, 0.25)
    assert prices.shape == (3, 9)
    # Each entry equals the scalar call.
    assert prices[1, 4] == bsm_price("call", 100.0, strikes[4], expiries[1], 0.03, 0.25)
    g = bsm_greeks("put", 100.0, strikes[None, :], expiries[:, None], 0.03, 0.25)
    assert all(v.shape == (3, 9) for v in g.values())
    np.testing.assert_array_equal(
        g["price"], bsm_price("put", 100.0, strikes, expiries[:, None], 0.03, 0.25)
    )


def test_accepts_enum_and_strings():
    a = bsm_price(OptionType.Put, 100, 110, 1, 0.05, 0.2)
    b = bsm_price("put", 100, 110, 1, 0.05, 0.2)
    c = bsm_price("PUT", 100, 110, 1, 0.05, 0.2)
    assert a == b == c


def test_rejects_unknown_kind():
    with pytest.raises(ValueError, match="option kind"):
        bsm_price("straddle", 100, 100, 1, 0.05, 0.2)


@pytest.mark.parametrize(
    "bad",
    [
        {"spot": 0.0},
        {"strike": -5.0},
        {"T": -0.1},
        {"sigma": -0.2},
        {"sigma": np.nan},
        {"r": np.inf},
    ],
)
def test_invalid_inputs_raise_value_error(bad):
    kwargs = {"spot": 100.0, "strike": 100.0, "T": 1.0, "r": 0.05, "sigma": 0.2}
    kwargs.update(bad)
    with pytest.raises(ValueError):
        bsm_price("call", **kwargs)
    with pytest.raises(ValueError):
        bsm_greeks("call", **kwargs)


def test_greeks_keyword_q_default_is_zero():
    with_q = optlib.bsm_price("call", 100, 100, 1, 0.05, 0.2, q=0.0)
    without = optlib.bsm_price("call", 100, 100, 1, 0.05, 0.2)
    assert with_q == without
