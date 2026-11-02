# Top-level build for optlib.
#
# Two ways this file gets used:
#   1. Directly, for C++ development:  cmake -S . -B build && cmake --build build && ctest --test-dir build
#   2. By scikit-build-core, when you run `pip install .`. It sets SKBUILD=ON and
#      only needs the Python extension, so we skip the C++ test suite in that case.

cmake_minimum_required(VERSION 3.20)

# Keep this version in sync with pyproject.toml. A Python test checks they match.
project(optlib VERSION 0.0.1 LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Language standard and global settings
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)   # fail loudly if the compiler can't do C++17
set(CMAKE_CXX_EXTENSIONS OFF)         # -std=c++17, not -std=gnu++17 (portable code only)

# Needed so the static library can be linked into the Python shared module.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Writes build/compile_commands.json, which clangd / clang-tidy / VS Code use.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
if(SKBUILD)
  set(_optlib_default_tests OFF)
  set(_optlib_default_python ON)
else()
  set(_optlib_default_tests ON)
  set(_optlib_default_python OFF)
endif()

option(OPTLIB_BUILD_TESTS  "Build the C++ unit tests"          ${_optlib_default_tests})
option(OPTLIB_BUILD_PYTHON "Build the pybind11 Python module"  ${_optlib_default_python})
option(OPTLIB_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

# ---------------------------------------------------------------------------
# Core C++ library (no Python dependency, so it can be tested and benchmarked
# on its own and reused from pure C++)
# ---------------------------------------------------------------------------
add_library(optlib STATIC
  src/rates.cpp
  src/version.cpp
)
add_library(optlib::optlib ALIAS optlib)

target_include_directories(optlib PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# Bake the project version into the library so C++ and Python agree on it.
target_compile_definitions(optlib PRIVATE OPTLIB_VERSION="${PROJECT_VERSION}")

# Warnings are applied per-target (not globally) so third-party code such as
# GoogleTest isn't held to our standards.
add_library(optlib_warnings INTERFACE)
if(MSVC)
  target_compile_options(optlib_warnings INTERFACE /W4)
  if(OPTLIB_WARNINGS_AS_ERRORS)
    target_compile_options(optlib_warnings INTERFACE /WX)
  endif()
else()
  target_compile_options(optlib_warnings INTERFACE
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion)
  if(OPTLIB_WARNINGS_AS_ERRORS)
    target_compile_options(optlib_warnings INTERFACE -Werror)
  endif()
endif()
target_link_libraries(optlib PRIVATE optlib_warnings)

# ---------------------------------------------------------------------------
# Python extension module
# ---------------------------------------------------------------------------
if(OPTLIB_BUILD_PYTHON)
  find_package(Python 3.11 REQUIRED COMPONENTS Interpreter Development.Module)
  find_package(pybind11 CONFIG REQUIRED)

  # Builds `_core` (e.g. _core.cpython-311-x86_64-linux-gnu.so). The leading
  # underscore marks it as private; users import the `optlib` package, whose
  # __init__.py re-exports what's public.
  pybind11_add_module(_core bindings/module.cpp)
  target_link_libraries(_core PRIVATE optlib::optlib optlib_warnings)

  # Install next to python/optlib/__init__.py inside the wheel.
  install(TARGETS _core DESTINATION optlib)
endif()

# ---------------------------------------------------------------------------
# C++ tests
# ---------------------------------------------------------------------------
if(OPTLIB_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests/cpp)
endif()
