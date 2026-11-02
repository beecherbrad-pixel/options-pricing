#include <gtest/gtest.h>

#include "optlib/version.hpp"

TEST(Version, IsNotEmpty) { EXPECT_FALSE(optlib::version().empty()); }
