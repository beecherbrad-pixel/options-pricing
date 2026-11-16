"""Implied-volatility tests from Python.

The C++ suite (tests/cpp/test_implied_vol.cpp) checks the solver in depth:
bounds, every status, and a 200-case round-trip grid. These tests check the
Python API and cross-check against SciPy's brentq, an independent root finder,
so a mistake would have to be made identically by two algorithms to slip through.
"""

import numpy as np
import pytest
from scipy.optimize import brentq

from optlib import IvStatus, OptionType, bsm_price, implied_vol


def lower_bound(kind, S, K, T, r, q):
    """No-arbitrage lower bound: discounted intrinsic value, floored at 0."""
    w = 1.0 if kind == "call" else -1.0
    return np.maximum(w * (S * np.exp(-q * T) - K * np.exp(-r * T)), 0.0)


def informative(kind, price, S, K, T, r, q):
    """Contracts whose price actually pins down sigma (see the C++ grid test).

    Deep in the money, a float64 price holds too few digits of time value;
    far out of the money, the price is ~0 and is correctly reported as being at
    the lower bound (sigma = 0). Neither this solver nor any other can recover
    the true sigma from such a price, so sigma-level checks skip them.
    """
    time_value = price - lower_bound(kind, S, K, T, r, q)
    return (time_value / price > 1e-8) & (time_value > 1e-10)


@pytest.fixture(scope="module")
def iv_inputs():
    """Seeded random contracts across moneyness, maturity, rates and vol."""
    rng = np.random.default_rng(20261115)
    n = 2_000
    S = rng.uniform(50, 150, n)
    return {
        "S": S,
        "K": S * np.exp(rng.uniform(-0.5, 0.5, n)),  # log-moneyness within +-0.5
        "T": rng.uniform(0.05, 3.0, n),
        "r": rng.uniform(-0.01, 0.08, n),
        "q": rng.uniform(0.0, 0.05, n),
        "sigma": rng.uniform(0.05, 1.5, n),
    }


def price_error(sigma, kind, price, S, K, T, r, q):
    """Root function for brentq: model price minus observed price."""
    return bsm_price(kind, S, K, T, r, sigma, q) - price


def market(kind, x):
    """Prices at the true sigma, plus the inputs implied_vol needs."""
    price = bsm_price(kind, x["S"], x["K"], x["T"], x["r"], x["sigma"], x["q"])
    return price, (x["S"], x["K"], x["T"], x["r"], x["q"])


@pytest.mark.parametrize("kind", ["call", "put"])
def test_round_trip_recovers_sigma(kind, iv_inputs):
    x = iv_inputs
    price, args = market(kind, x)
    iv = implied_vol(kind, price, *args)
    mask = informative(kind, price, *args)
    assert mask.mean() > 0.9  # the mask must not hide most of the sample
    np.testing.assert_allclose(iv[mask], x["sigma"][mask], rtol=0, atol=1e-8)


@pytest.mark.parametrize("kind", ["call", "put"])
def test_round_trip_reprices_every_contract(kind, iv_inputs):
    x = iv_inputs
    price, args = market(kind, x)
    res = implied_vol(kind, price, *args, full_output=True)
    # Every contract either solves or, with a price of ~0, sits at the lower bound.
    ok = (res["status"] == int(IvStatus.Converged)) | (
        res["status"] == int(IvStatus.PriceAtLowerBound)
    )
    assert np.all(ok)
    S, K, T, r, q = args
    repriced = bsm_price(kind, S, K, T, r, res["sigma"], q)
    # atol must cover the solver's at-the-bound tolerance, 1e-14 * max(S e^{-qT}, K e^{-rT}),
    # which is up to ~3e-12 for these inputs: a price that small is reported as sigma = 0.
    np.testing.assert_allclose(repriced, price, rtol=1e-10, atol=1e-11)


@pytest.mark.parametrize("kind", ["call", "put"])
def test_matches_scipy_brentq(kind, iv_inputs):
    x = iv_inputs
    price, args = market(kind, x)
    for i in np.flatnonzero(informative(kind, price, *args))[:50]:
        S, K, T, r, q = (a[i] for a in args)
        p = price[i]
        ref = brentq(price_error, 1e-6, 10.0, args=(kind, p, S, K, T, r, q), xtol=1e-14)
        assert implied_vol(kind, p, S, K, T, r, q) == pytest.approx(ref, abs=1e-8)


@pytest.mark.parametrize(
    ("kind", "price", "status"),
    [
        ("call", 18.0, IvStatus.PriceBelowLowerBound),  # intrinsic is ~23.9
        ("call", -0.5, IvStatus.PriceBelowLowerBound),  # negative: status, not an error
        ("call", 100.0, IvStatus.PriceAtOrAboveUpperBound),  # at or above S e^{-qT}
        ("put", 80.0, IvStatus.PriceAtOrAboveUpperBound),  # above K e^{-rT}, K = 80
    ],
)
def test_out_of_bounds_prices_return_nan(kind, price, status):
    S, K, T, r, q = 100.0, 80.0, 1.0, 0.05, 0.0
    res = implied_vol(kind, price, S, K, T, r, q, full_output=True)
    assert np.isnan(res["sigma"])
    assert res["status"] == int(status)
    assert np.isnan(implied_vol(kind, price, S, K, T, r, q))


@pytest.mark.parametrize(("kind", "K"), [("call", 80.0), ("put", 80.0), ("put", 120.0)])
def test_zero_time_value_gives_zero_sigma(kind, K):
    price = bsm_price(kind, 100.0, K, 1.0, 0.05, 0.0)
    res = implied_vol(kind, price, 100.0, K, 1.0, 0.05, full_output=True)
    assert res["sigma"] == 0.0
    assert res["iterations"] == 0
    assert res["status"] == int(IvStatus.PriceAtLowerBound)


def test_sigma_above_search_range_is_reported():
    price = bsm_price("call", 100.0, 100.0, 1.0, 0.05, 12.0)  # beyond sigma_max = 10
    res = implied_vol("call", price, 100.0, 100.0, 1.0, 0.05, full_output=True)
    assert np.isnan(res["sigma"])
    assert res["status"] == int(IvStatus.SigmaAboveMax)


def test_broadcast_strike_by_expiry_grid():
    strikes = np.linspace(80, 120, 41)
    expiries = np.array([0.25, 0.5, 1.0])
    prices = bsm_price("call", 100.0, strikes[None, :], expiries[:, None], 0.03, 0.25)
    iv = implied_vol("call", prices, 100.0, strikes[None, :], expiries[:, None], 0.03)
    assert iv.shape == (3, 41)
    np.testing.assert_allclose(iv, 0.25, rtol=0, atol=1e-10)
    res = implied_vol("call", prices, 100.0, strikes, expiries[:, None], 0.03, full_output=True)
    assert all(v.shape == (3, 41) for v in res.values())


def test_scalar_inputs_return_python_types():
    price = bsm_price("call", 100, 100, 1, 0.05, 0.2)
    assert isinstance(implied_vol("call", price, 100, 100, 1, 0.05), float)
    res = implied_vol("call", price, 100, 100, 1, 0.05, full_output=True)
    assert isinstance(res["sigma"], float)
    assert isinstance(res["iterations"], int)
    assert isinstance(res["status"], int)


def test_accepts_enum_and_strings():
    price = bsm_price("put", 100, 110, 1, 0.05, 0.2)
    a = implied_vol(OptionType.Put, price, 100, 110, 1, 0.05)
    b = implied_vol("put", price, 100, 110, 1, 0.05)
    c = implied_vol("PUT", price, 100, 110, 1, 0.05)
    assert a == b == c
    assert a == pytest.approx(0.2, abs=1e-12)


def test_full_output_is_keyword_only():
    with pytest.raises(TypeError):
        implied_vol("call", 10.0, 100, 100, 1, 0.05, 0.0, True)


@pytest.mark.parametrize(
    "bad",
    [
        {"spot": 0.0},
        {"strike": -5.0},
        {"T": 0.0},
        {"price": np.nan},
        {"r": np.inf},
    ],
)
def test_invalid_inputs_raise_value_error(bad):
    kwargs = {"price": 10.0, "spot": 100.0, "strike": 100.0, "T": 1.0, "r": 0.05}
    kwargs.update(bad)
    with pytest.raises(ValueError):
        implied_vol("call", **kwargs)
