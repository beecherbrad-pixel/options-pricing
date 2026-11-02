include(FetchContent)

# GoogleTest is downloaded at configure time and pinned to a release, so every
# machine (yours, CI) tests against the same version.
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.15.2   # commit b514bdc898e2951020cbdca1304b75f5950d1f59
  GIT_SHALLOW    TRUE
)
# On Windows/MSVC, make GoogleTest use the same C runtime as our code.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(optlib_tests
  test_rates.cpp
  test_version.cpp
)
target_link_libraries(optlib_tests PRIVATE optlib::optlib optlib_warnings GTest::gtest_main)

# Registers each TEST() as its own CTest test, so `ctest` lists them individually.
include(GoogleTest)
gtest_discover_tests(optlib_tests)
