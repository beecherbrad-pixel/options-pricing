#include "optlib/version.hpp"

#ifndef OPTLIB_VERSION
#error "OPTLIB_VERSION must be defined by the build system (see CMakeLists.txt)"
#endif

namespace optlib {

std::string_view version() noexcept { return OPTLIB_VERSION; }

}  // namespace optlib
