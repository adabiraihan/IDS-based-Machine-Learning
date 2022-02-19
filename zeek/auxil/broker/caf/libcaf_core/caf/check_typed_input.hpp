// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/fwd.hpp"
#include "caf/replies_to.hpp"

#include "caf/detail/type_list.hpp"

namespace caf {

template <class T>
struct signatures_of {
  using type = typename std::remove_pointer<T>::type::signatures;
};

template <class T>
using signatures_of_t = typename signatures_of<T>::type;

template <class T>
constexpr bool statically_typed() {
  return !std::is_same<
    none_t, typename std::remove_pointer<T>::type::signatures>::value;
}

template <class T>
struct is_void_response : std::false_type {};

template <>
struct is_void_response<void> : std::true_type {};

template <>
struct is_void_response<detail::type_list<void>> : std::true_type {};

// true for the purpose of type checking performed by send()
template <>
struct is_void_response<none_t> : std::true_type {};

} // namespace caf
