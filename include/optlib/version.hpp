#pragma once

#include <string_view>

namespace optlib {

/// Library version, e.g. "0.0.1". Set from project(VERSION ...) in CMakeLists.txt.
[[nodiscard]] std::string_view version() noexcept;

}  // namespace optlib
