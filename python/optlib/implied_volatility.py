"""Black-Scholes-Merton implied volatility for European options.

Thin Python layer over the C++ solver (safeguarded Newton with a bisection
fallback): it accepts "call"/"put" strings as well as ``OptionType`` values,
converts scalar results back to plain Python numbers, and holds the docstring.
All numerical work happens in C++.
"""

from __future__ import annotations

import numpy as np
from numpy.typing import ArrayLike

from optlib import _core
from optlib.black_scholes import Kind, _maybe_scalar, _to_option_type


def _maybe_int(x: np.ndarray) -> int | np.ndarray:
    return int(x) if np.ndim(x) == 0 else x


def implied_vol(
    kind: Kind,
    price: ArrayLike,
    spot: ArrayLike,
    strike: ArrayLike,
    T: ArrayLike,
    r: ArrayLike,
    q: ArrayLike = 0.0,
    *,
    full_output: bool = False,
) -> float | np.ndarray | dict[str, float | int | np.ndarray]:
    """Black-Scholes-Merton implied volatility of a European option.

    Finds the volatility at which :func:`bsm_price` matches ``price``.

    Parameters
    ----------
    kind : {"call", "put"} or OptionType
    price : float or array
        Observed option price.
    spot, strike : float or array
        Underlying price and strike, in the same units as ``price``.
    T : float or array
        Time to expiry in years (e.g. ``days / 365``).
    r : float or array
        Continuously compounded risk-free rate (0.05 = 5%).
    q : float or array, default 0
        Continuous dividend yield. Use ``q = r`` for Black's model on futures.
    full_output : bool, default False
        If True, also return the iteration count and solver status.

    Array inputs broadcast like NumPy; e.g. ``strike=np.linspace(80, 120, 41)``
    with a matching ``price`` array solves a whole strike ladder in one call.

    Returns
    -------
    sigma : float or ndarray
        Annualised implied volatility as a decimal (0.20 = 20%). A float if every
        input is a scalar, otherwise an ndarray. It is 0 when the price equals the
        no-arbitrage lower bound, and NaN whenever no implied volatility was found
        (see the statuses below).
    dict, if ``full_output`` is True
        Keys ``sigma`` (as above), ``iterations`` (solver iterations, 0 when none
        were needed) and ``status``: integer codes of :class:`IvStatus`, compared
        with e.g. ``status == int(IvStatus.Converged)``:

        - ``Converged``: solved; sigma is the implied volatility.
        - ``PriceAtLowerBound``: no time value; sigma is exactly 0.
        - ``PriceBelowLowerBound``: below the no-arbitrage lower bound; NaN.
        - ``PriceAtOrAboveUpperBound``: at or above the upper bound; NaN.
        - ``SigmaAboveMax``: valid price, but sigma exceeds 10 (1000%); NaN.
        - ``MaxIterationsExceeded``: solver did not converge; NaN.

    Raises
    ------
    ValueError
        If spot or strike <= 0, T <= 0, or any input is not finite. Out-of-bounds
        prices (including negative ones) do not raise; they return NaN.

    Notes
    -----
    Deep in-the-money prices are mostly intrinsic value, so a float64 price
    carries few digits of time value: the result always reprices the input, but
    sigma itself may be accurate only to ~1e-5 in extreme cases.
    """
    raw = _core.implied_vol(_to_option_type(kind), price, spot, strike, T, r, q)
    if not full_output:
        return _maybe_scalar(raw["sigma"])
    return {
        "sigma": _maybe_scalar(raw["sigma"]),
        "iterations": _maybe_int(raw["iterations"]),
        "status": _maybe_int(raw["status"]),
    }
