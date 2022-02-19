#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <caf/default_sum_type_access.hpp>
#include <caf/fwd.hpp>
#include <caf/sum_type_access.hpp>
#include <caf/variant.hpp>

#include "broker/address.hh"
#include "broker/bad_variant_access.hh"
#include "broker/convert.hh"
#include "broker/detail/type_traits.hh"
#include "broker/enum_value.hh"
#include "broker/fwd.hh"
#include "broker/none.hh"
#include "broker/optional.hh"
#include "broker/port.hh"
#include "broker/subnet.hh"
#include "broker/time.hh"

namespace broker {

class data;

/// A container of sequential data.
using vector = std::vector<data>;

/// @relates vector
bool convert(const vector& v, std::string& str);

/// An associative, ordered container of unique keys.
using set = std::set<data>;

/// @relates set
bool convert(const set& s, std::string& str);

/// An associative, ordered container that maps unique keys to values.
using table = std::map<data, data>;

/// @relates table
bool convert(const table& t, std::string& str);

using data_variant = caf::variant<
  none,
  boolean,
  count,
  integer,
  real,
  std::string,
  address,
  subnet,
  port,
  timestamp,
  timespan,
  enum_value,
  set,
  table,
  vector
>;

/// A variant class that may store the data associated with one of several
/// different primitive or compound types.
class data {
public:
  using types = typename data_variant::types;

  enum class type : uint8_t {
    address,
    boolean,
    count,
    enum_value,
    integer,
    none,
    port,
    real,
    set,
    string,
    subnet,
    table,
    timespan,
    timestamp,
    vector
  };

	template <class T>
	using from = detail::conditional_t<
        std::is_floating_point<T>::value,
        real,
        detail::conditional_t<
          std::is_same<T, bool>::value,
          boolean,
          detail::conditional_t<
            std::is_unsigned<T>::value,
            count,
            detail::conditional_t<
              std::is_signed<T>::value,
              integer,
              detail::conditional_t<
                std::is_convertible<T, std::string>::value,
                std::string,
                detail::conditional_t<
                  std::is_same<T, timestamp>::value
                    || std::is_same<T, timespan>::value
                    || std::is_same<T, enum_value>::value
	                  || std::is_same<T, address>::value
                    || std::is_same<T, subnet>::value
                    || std::is_same<T, port>::value
                    || std::is_same<T, broker::set>::value
                    || std::is_same<T, table>::value
                    || std::is_same<T, vector>::value,
                  T,
                  std::false_type
                >
              >
            >
          >
        >
      >;

  /// Default-constructs an empty data value in `none` state.
  data(none = nil) {
    // nop
  }

  /// Constructs a data value from one of the possible data types.
	template <
	  class T,
	  class = detail::disable_if_t<
              detail::is_same_or_derived<data, T>::value
                || std::is_same<
                     from<detail::decay_t<T>>,
                     std::false_type
                   >::value
            >
	>
	data(T&& x) : data_(from<detail::decay_t<T>>(std::forward<T>(x))) {
	  // nop
	}

  /// Returns a string representation of the stored type.
  const char* get_type_name() const;

  /// Returns the type tag of the stored type.
  type get_type() const;

  static data from_type(type);

  // Needed by caf::default_variant_access.
  data_variant& get_data() {
    return data_;
  }

  // Needed by caf::default_variant_access.
  const data_variant& get_data() const {
    return data_;
  }

private:
  data_variant data_;
};

namespace detail {

template <data::type Value>
using data_tag_token = std::integral_constant<data::type, Value>;

template <class T>
struct data_tag_oracle;

template <>
struct data_tag_oracle<std::string> : data_tag_token<data::type::string> {};

#define DATA_TAG_ORACLE(type_name)                                             \
  template <>                                                                  \
  struct data_tag_oracle<type_name> : data_tag_token<data::type::type_name> {}

DATA_TAG_ORACLE(none);
DATA_TAG_ORACLE(boolean);
DATA_TAG_ORACLE(count);
DATA_TAG_ORACLE(integer);
DATA_TAG_ORACLE(real);
DATA_TAG_ORACLE(address);
DATA_TAG_ORACLE(subnet);
DATA_TAG_ORACLE(port);
DATA_TAG_ORACLE(timestamp);
DATA_TAG_ORACLE(timespan);
DATA_TAG_ORACLE(enum_value);
DATA_TAG_ORACLE(set);
DATA_TAG_ORACLE(table);
DATA_TAG_ORACLE(vector);

#undef DATA_TAG_ORACLE

} // namespace detail

/// Returns the `data::type` tag for `T`.
/// @relates data
template <class T>
constexpr data::type data_tag() {
  return detail::data_tag_oracle<T>::value;
}

/// @relates data
template <class Inspector>
bool inspect(Inspector& f, data::type& x) {
  auto get = [&] { return static_cast<uint8_t>(x); };
  auto set = [&](uint8_t val) {
    if (val <= static_cast<uint8_t>(data::type::vector)) {
      x = static_cast<data::type>(val);
      return true;
    } else {
      return false;
    }
  };
  return f.apply(get, set);
}

/// @relates data
template <class Inspector>
bool inspect(Inspector& f, data& x) {
  return f.object(x).fields(f.field("data", x.get_data()));
}

/// @relates data
bool convert(const data& d, std::string& str);

/// @relates data
bool convert(const data& d, caf::node_id& node);

/// @relates data
bool convert(const caf::node_id& node, data& d);

/// @relates data
inline std::string to_string(const broker::data& d) {
  std::string s;
  convert(d, s);
  return s;
}

inline bool operator<(const data& x, const data& y) {
  return x.get_data() < y.get_data();
}

inline bool operator<=(const data& x, const data& y) {
  return x.get_data() <= y.get_data();
}

inline bool operator>(const data& x, const data& y) {
  return x.get_data() > y.get_data();
}

inline bool operator>=(const data& x, const data& y) {
  return x.get_data() >= y.get_data();
}

inline bool operator==(const data& x, const data& y) {
  return x.get_data() == y.get_data();
}

inline bool operator!=(const data& x, const data& y) {
  return x.get_data() != y.get_data();
}

// --- compatibility/wrapper functionality (may be removed later) --------------

template <class T>
inline bool is(const data& v) {
  return caf::holds_alternative<T>(v);
}

template <class T>
inline T* get_if(data& d) {
  return caf::get_if<T>(&d);
}

template <class T>
inline const T* get_if(const data& d) {
  return caf::get_if<T>(&d);
}

template <class T>
inline T& get(data& d) {
  if ( auto rval = caf::get_if<T>(&d) )
    return *rval;
  throw bad_variant_access{};
}

template <class T>
inline const T& get(const data& d) {
  if ( auto rval = caf::get_if<T>(&d) )
    return *rval;
  throw bad_variant_access{};
}

template <class Visitor>
typename detail::remove_reference_t<Visitor>::result_type
inline visit(Visitor&& visitor, data d) {
  return caf::visit(std::forward<Visitor>(visitor), std::move(d));
}

// --- convenience functions ---------------------------------------------------

/// Wildcard for `contains` to skip type check at a specific location.
struct any_type {};

template <class T>
bool exact_match_or_can_convert_to(const data& x) {
  if constexpr (detail::is_complete<detail::data_tag_oracle<T>>)
    return is<T>(x);
  else if constexpr (std::is_same<any_type, T>::value)
    return true;
  else
    return can_convert_to<T>(x);
}

template <size_t... Is, class... Ts>
bool contains_impl(const vector& xs, std::index_sequence<Is...>,
                   detail::type_list<Ts...>) {
  return xs.size() == sizeof...(Ts)
         && (exact_match_or_can_convert_to<Ts>(xs[Is]) && ...);
}

/// Checks whether `xs` contains values of types `Ts...`. Performs "fuzzy"
/// matching by calling `can_convert_to<T>` for any `T` that is not part of the
/// variant.
template <class...Ts>
bool contains(const vector& xs) {
  return contains_impl(xs, std::make_index_sequence<sizeof...(Ts)>{},
                       detail::type_list<Ts...>{});
}

template <class...Ts>
bool contains(const data& d) {
  if (auto xs = get_if<vector>(d))
    return contains<Ts...>(*xs);
  return false;
}
} // namespace broker

namespace broker::detail {

size_t fnv_hash(const broker::data& x);

size_t fnv_hash(const broker::set& x);

size_t fnv_hash(const broker::vector& x);

size_t fnv_hash(const broker::table::value_type& x);

size_t fnv_hash(const broker::table& x);

} // namespace broker::detail

// --- treat data as sum type (equivalent to variant) --------------------------

namespace caf {

template <>
struct sum_type_access<broker::data> : default_sum_type_access<broker::data> {};

} // namespace caf

// --- implementations of std::hash --------------------------------------------

namespace std {

template <>
struct hash<broker::data> {
  size_t operator()(const broker::data& x) const {
    return broker::detail::fnv_hash(x);
  }
};

template <>
struct hash<broker::set> {
  size_t operator()(const broker::set& x) const{
    return broker::detail::fnv_hash(x);
  }
};

template <>
struct hash<broker::vector> {
  size_t operator()(const broker::vector& x) const{
    return broker::detail::fnv_hash(x);
  }
};

template <>
struct hash<broker::table::value_type> {
  size_t operator()(const broker::table::value_type& x) const{
    return broker::detail::fnv_hash(x);
  }
};

template <>
struct hash<broker::table> {
  size_t operator()(const broker::table& x) const{
    return broker::detail::fnv_hash(x);
  }
};

} // namespace std
