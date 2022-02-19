// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <utility>

#include "caf/detail/int_list.hpp"
#include "caf/detail/type_list.hpp"

namespace caf::detail {

// this utterly useless function works around a bug in Clang that causes
// the compiler to reject the trailing return type of apply_args because
// "get" is not defined (it's found during ADL)
template <long Pos, class... Ts>
typename tl_at<type_list<Ts...>, Pos>::type get(const type_list<Ts...>&);

template <class F, long... Is, class Tuple>
decltype(auto) apply_args(F& f, detail::int_list<Is...>, Tuple& tup) {
  return f(get<Is>(tup)...);
}

template <class F, size_t... Is, class Tuple>
decltype(auto) apply_args(F& f, std::index_sequence<Is...>, Tuple& tup) {
  return f(get<Is>(tup)...);
}

template <class F, long... Is, class Tuple>
decltype(auto) apply_args(F& f, Tuple& tup) {
  auto token = get_indices(tup);
  return apply_args(f, token, tup);
}

template <class F, long... Is, class Tuple>
auto apply_moved_args(F& f, detail::int_list<Is...>, Tuple& tup)
  -> decltype(f(std::move(get<Is>(tup))...)) {
  return f(std::move(get<Is>(tup))...);
}

template <class F, class Tuple, class... Ts>
auto apply_args_prefixed(F& f, detail::int_list<>, Tuple&, Ts&&... xs)
  -> decltype(f(std::forward<Ts>(xs)...)) {
  return f(std::forward<Ts>(xs)...);
}

template <class F, long... Is, class Tuple, class... Ts>
auto apply_args_prefixed(F& f, detail::int_list<Is...>, Tuple& tup, Ts&&... xs)
  -> decltype(f(std::forward<Ts>(xs)..., get<Is>(tup)...)) {
  return f(std::forward<Ts>(xs)..., get<Is>(tup)...);
}

template <class F, class Tuple, class... Ts>
auto apply_moved_args_prefixed(F& f, detail::int_list<>, Tuple&, Ts&&... xs)
  -> decltype(f(std::forward<Ts>(xs)...)) {
  return f(std::forward<Ts>(xs)...);
}

template <class F, long... Is, class Tuple, class... Ts>
auto apply_moved_args_prefixed(F& f, detail::int_list<Is...>, Tuple& tup,
                               Ts&&... xs)
  -> decltype(f(std::forward<Ts>(xs)..., std::move(get<Is>(tup))...)) {
  return f(std::forward<Ts>(xs)..., std::move(get<Is>(tup))...);
}

template <class F, long... Is, class Tuple, class... Ts>
auto apply_args_suffxied(F& f, detail::int_list<Is...>, Tuple& tup, Ts&&... xs)
  -> decltype(f(get<Is>(tup)..., std::forward<Ts>(xs)...)) {
  return f(get<Is>(tup)..., std::forward<Ts>(xs)...);
}

} // namespace caf::detail
