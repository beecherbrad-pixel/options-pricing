"""optlib: options pricing with a C++17 core and a Python API."""

from optlib._core import discount_factor
from optlib._core import version as _core_version

__version__ = _core_version()

__all__ = ["__version__", "discount_factor"]
