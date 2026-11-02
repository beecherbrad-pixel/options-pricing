#pragma once

// Basic vocabulary types shared by every pricing model.

namespace optlib {

/// Call or put.
///
/// `enum class` (a "scoped enum") instead of a plain `enum` or a `bool is_call`:
///   - The names don't leak into the surrounding namespace (OptionType::Call, not Call).
///   - No implicit conversion to int, so passing an OptionType where a number is
///     expected (or vice versa) is a compile error rather than a silent bug.
///   - Call sites read clearly: price(OptionType::Put, ...) vs price(false, ...).
enum class OptionType { Call, Put };

/// +1 for a call, -1 for a put.
///
/// Many Black-Scholes formulas are identical for calls and puts up to this sign
/// (often written as omega or phi in textbooks), which lets us write each
/// formula once instead of twice.
[[nodiscard]] constexpr double sign(OptionType type) noexcept {
  return type == OptionType::Call ? 1.0 : -1.0;
}

}  // namespace optlib
