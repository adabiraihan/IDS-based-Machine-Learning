// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/config_value.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/move_if_not_ptr.hpp"
#include "caf/dictionary.hpp"
#include "caf/optional.hpp"
#include "caf/raise_error.hpp"
#include "caf/string_view.hpp"
#include "caf/sum_type.hpp"

namespace caf {

/// Software options stored as key-value pairs.
/// @relates config_value
using settings = dictionary<config_value>;

/// @relates config_value
CAF_CORE_EXPORT std::string to_string(const settings& xs);

/// Tries to retrieve the value associated to `name` from `xs`.
/// @relates config_value
CAF_CORE_EXPORT const config_value* get_if(const settings* xs,
                                           string_view name);

/// Tries to retrieve the value associated to `name` from `xs`.
/// @relates config_value
template <class T>
auto get_if(const settings* xs, string_view name) {
  auto value = get_if(xs, name);
  using result_type = decltype(get_if<T>(value));
  return value ? get_if<T>(value) : result_type{};
}

/// Returns whether `xs` associates a value of type `T` to `name`.
/// @relates config_value
template <class T>
bool holds_alternative(const settings& xs, string_view name) {
  if (auto value = get_if(&xs, name))
    return holds_alternative<T>(*value);
  else
    return false;
}

/// Retrieves the value associated to `name` from `xs`.
/// @relates actor_system_config
template <class T>
T get(const settings& xs, string_view name) {
  auto result = get_if<T>(&xs, name);
  CAF_ASSERT(result);
  return detail::move_if_not_ptr(result);
}

/// Retrieves the value associated to `name` from `xs` or returns `fallback`.
/// @relates actor_system_config
template <class To = get_or_auto_deduce, class Fallback>
auto get_or(const settings& xs, string_view name, Fallback&& fallback) {
  if (auto ptr = get_if(&xs, name)) {
    return get_or<To>(*ptr, std::forward<Fallback>(fallback));
  } else if constexpr (std::is_same<To, get_or_auto_deduce>::value) {
    using guide = get_or_deduction_guide<std::decay_t<Fallback>>;
    return guide::convert(std::forward<Fallback>(fallback));
  } else {
    return To{std::forward<Fallback>(fallback)};
  }
}

/// Tries to retrieve the value associated to `name` from `xs` as an instance of
/// type `T`.
/// @relates actor_system_config
template <class T>
expected<T> get_as(const settings& xs, string_view name) {
  if (auto ptr = get_if(&xs, name))
    return get_as<T>(*ptr);
  else
    return {sec::no_such_key};
}

/// @private
CAF_CORE_EXPORT config_value& put_impl(settings& dict,
                                       const std::vector<string_view>& path,
                                       config_value& value);

/// @private
CAF_CORE_EXPORT config_value& put_impl(settings& dict, string_view key,
                                       config_value& value);

/// Converts `value` to a `config_value` and assigns it to `key`.
/// @param dict Dictionary of key-value pairs.
/// @param key Human-readable nested keys in the form `category.key`.
/// @param value New value for given `key`.
template <class T>
config_value& put(settings& dict, string_view key, T&& value) {
  config_value tmp{std::forward<T>(value)};
  return put_impl(dict, key, tmp);
}

/// Converts `value` to a `config_value` and assigns it to `key` unless `xs`
/// already contains `key` (does nothing in this case).
/// @param xs Dictionary of key-value pairs.
/// @param key Human-readable nested keys in the form `category.key`.
/// @param value New value for given `key`.
template <class T>
void put_missing(settings& xs, string_view key, T&& value) {
  if (get_if(&xs, key) != nullptr)
    return;
  config_value tmp{std::forward<T>(value)};
  put_impl(xs, key, tmp);
}

/// Inserts a new list named `name` into the dictionary `xs` and returns
/// a reference to it. Overrides existing entries with the same name.
CAF_CORE_EXPORT config_value::list& put_list(settings& xs, std::string name);

/// Inserts a new list named `name` into the dictionary `xs` and returns
/// a reference to it. Overrides existing entries with the same name.
CAF_CORE_EXPORT config_value::dictionary& put_dictionary(settings& xs,
                                                         std::string name);

} // namespace caf
