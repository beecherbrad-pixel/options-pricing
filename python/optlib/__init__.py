"""optlib: options pricing with a C++17 core and a Python API."""

from optlib._core import OptionType, discount_factor
from optlib._core import version as _core_version
from optlib.black_scholes import bsm_greeks, bsm_price

__version__ = _core_version()

__all__ = ["OptionType", "__version__", "bsm_greeks", "bsm_price", "discount_factor"]
