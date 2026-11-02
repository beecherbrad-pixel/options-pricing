"""Smoke tests: the extension builds, imports, and the bindings behave."""

import importlib.metadata
import math
import tomllib
from pathlib import Path

import pytest

import optlib

REPO_ROOT = Path(__file__).resolve().parents[2]


def test_version_matches_package_metadata():
    assert optlib.__version__ == importlib.metadata.version("optlib")


def test_version_matches_pyproject():
    # Guards against bumping one version string and forgetting the other.
    pyproject = tomllib.loads((REPO_ROOT / "pyproject.toml").read_text())
    assert optlib.__version__ == pyproject["project"]["version"]


@pytest.mark.parametrize(
    ("r", "T"),
    [(0.0, 1.0), (0.05, 1.0), (0.05, 0.0), (0.03, 10.0), (-0.005, 2.0)],
)
def test_discount_factor_matches_python(r, T):
    assert optlib.discount_factor(r, T) == pytest.approx(math.exp(-r * T), rel=1e-15)


def test_discount_factor_accepts_keywords():
    assert optlib.discount_factor(r=0.05, T=1.0) == pytest.approx(math.exp(-0.05))


def test_discount_factor_rejects_wrong_type():
    with pytest.raises(TypeError):
        optlib.discount_factor("0.05", 1.0)
