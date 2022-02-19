
// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <limits>

#include "caf/detail/parser/ascii_to_int.hpp"
#include "caf/detail/type_traits.hpp"

namespace caf::detail::parser {

// Subtracs integers when parsing negative integers.
// @returns `false` on an underflow, otherwise `true`.
// @pre `isdigit(c) || (Base == 16 && isxdigit(c))`
// @warning can leave `x` in an intermediate state when retuning `false`
template <int Base, class T>
bool sub_ascii(T& x, char c, enable_if_tt<std::is_integral<T>, int> u = 0) {
  CAF_IGNORE_UNUSED(u);
  if (x < (std::numeric_limits<T>::min() / Base))
    return false;
  x *= static_cast<T>(Base);
  ascii_to_int<Base, T> f;
  auto y = f(c);
  if (x < (std::numeric_limits<T>::min() + y))
    return false;
  x -= static_cast<T>(y);
  return true;
}

template <int Base, class T>
bool sub_ascii(T& x, char c,
               enable_if_tt<std::is_floating_point<T>, int> u = 0) {
  CAF_IGNORE_UNUSED(u);
  ascii_to_int<Base, T> f;
  x = static_cast<T>((x * Base) - f(c));
  return true;
}

} // namespace caf::detail::parser
