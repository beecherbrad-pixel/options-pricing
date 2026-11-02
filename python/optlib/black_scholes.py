"""Black-Scholes-Merton prices and Greeks for European options.

Thin Python layer over the C++ core: it accepts "call"/"put" strings as well as
``OptionType`` values, converts scalar results back to plain floats, and holds
the docstrings. All numerical work happens in C++.
"""

from __future__ import annotations

from typing import Literal

import numpy as np
from numpy.typing import ArrayLike

from optlib import _core
from optlib._core import OptionType

Kind = OptionType | Literal["call", "put"]

_GREEK_NAMES = ("price", "delta", "gamma", "vega", "theta", "rho")


def _to_option_type(kind: Kind) -> OptionType:
    if isinstance(kind, OptionType):
        return kind
    if isinstance(kind, str):
        lowered = kind.lower()
        if lowered == "call":
            return OptionType.Call
        if lowered == "put":
            return OptionType.Put
    raise ValueError(f"option kind must be 'call', 'put' or an OptionType, got {kind!r}")


def _maybe_scalar(x: np.ndarray) -> float | np.ndarray:
    return float(x) if np.ndim(x) == 0 else x


def bsm_price(
    kind: Kind,
    spot: ArrayLike,
    strike: ArrayLike,
    T: ArrayLike,
    r: ArrayLike,
    sigma: ArrayLike,
    q: ArrayLike = 0.0,
) -> float | np.ndarray:
    """Black-Scholes-Merton price of a European option.

    Parameters
    ----------
    kind : {"call", "put"} or OptionType
    spot, strike : float or array
        Underlying price and strike, in the same units.
    T : float or array
        Time to expiry in years (e.g. ``days / 365``).
    r : float or array
        Continuously compounded risk-free rate (0.05 = 5%).
    sigma : float or array
        Annualised volatility (0.20 = 20%).
    q : float or array, default 0
        Continuous dividend yield. Use ``q = r`` for Black's model on futures.

    Array inputs broadcast like NumPy; e.g. ``strike=np.linspace(80, 120, 41)``
    prices a whole strike ladder in one call.

    Returns
    -------
    float if every input is a scalar, otherwise ndarray.

    Raises
    ------
    ValueError
        If spot or strike <= 0, T or sigma < 0, or any input is not finite.
    """
    return _maybe_scalar(_core.bsm_price(_to_option_type(kind), spot, strike, T, r, sigma, q))


def bsm_greeks(
    kind: Kind,
    spot: ArrayLike,
    strike: ArrayLike,
    T: ArrayLike,
    r: ArrayLike,
    sigma: ArrayLike,
    q: ArrayLike = 0.0,
) -> dict[str, float | np.ndarray]:
    """Black-Scholes-Merton price and Greeks.

    Same inputs as :func:`bsm_price`. Returns a dict with keys ``price``,
    ``delta``, ``gamma``, ``vega``, ``theta``, ``rho``.

    Units are per 1.00 change in the input and per year:

    - ``vega`` per 1.00 of vol; divide by 100 for "per vol point".
    - ``theta`` per year of calendar time (dV/dt); divide by 365 for "per day".
    - ``rho`` per 1.00 of rate; divide by 10,000 for "per basis point".
    """
    raw = _core.bsm_greeks(_to_option_type(kind), spot, strike, T, r, sigma, q)
    return {name: _maybe_scalar(raw[name]) for name in _GREEK_NAMES}
