// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "caf/byte_address.hpp"
#include "caf/detail/comparable.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

namespace caf {

class CAF_CORE_EXPORT ipv4_address : public byte_address<ipv4_address>,
                                     detail::comparable<ipv4_address> {
public:
  // -- constants --------------------------------------------------------------

  static constexpr size_t num_bytes = 4;

  // -- member types -----------------------------------------------------------

  using super = byte_address<ipv4_address>;

  using array_type = std::array<uint8_t, num_bytes>;

  // -- constructors, destructors, and assignment operators --------------------

  ipv4_address();

  explicit ipv4_address(array_type bytes);

  /// Constructs an IPv4 address from bits in network byte order.
  static ipv4_address from_bits(uint32_t bits) {
    ipv4_address result;
    result.bits(bits);
    return result;
  }

  // -- properties -------------------------------------------------------------

  /// Returns whether this is a loopback address.
  bool is_loopback() const noexcept;

  /// Returns whether this is a multicast address.
  bool is_multicast() const noexcept;

  /// Returns the bits of the IP address in a single integer arranged in network
  /// byte order.
  uint32_t bits() const noexcept {
    return bits_;
  }

  /// Sets all bits of the IP address with a single 32-bit write. Expects
  /// argument in network byte order.
  void bits(uint32_t value) noexcept {
    bits_ = value;
  }

  /// Returns the bytes of the IP address as array.
  array_type& bytes() noexcept {
    return bytes_;
  }

  /// Returns the bytes of the IP address as array.
  const array_type& bytes() const noexcept {
    return bytes_;
  }

  /// Alias for `bytes()`.
  array_type& data() noexcept {
    return bytes_;
  }

  /// Alias for `bytes()`.
  const array_type& data() const noexcept {
    return bytes_;
  }

  // -- comparison -------------------------------------------------------------

  /// Returns a negative number if `*this < other`, zero if `*this == other`
  /// and a positive number if `*this > other`.
  int compare(ipv4_address other) const noexcept;

  // -- inspection -------------------------------------------------------------

  template <class Inspector>
  friend bool inspect(Inspector& f, ipv4_address& x) {
    return f.object(x).fields(f.field("value", x.bits_));
  }

private:
  // -- member variables -------------------------------------------------------

  union {
    uint32_t bits_;
    array_type bytes_;
  };

  // -- sanity checks ----------------------------------------------------------

  static_assert(sizeof(array_type) == sizeof(uint32_t),
                "array<char, 4> has a different size than uint32");
};

// -- related free functions ---------------------------------------------------

/// Convenience function for creating an IPv4 address from octets.
/// @relates ipv4_address
CAF_CORE_EXPORT ipv4_address make_ipv4_address(uint8_t oct1, uint8_t oct2,
                                               uint8_t oct3, uint8_t oct4);

/// Returns a human-readable string representation of the address.
/// @relates ipv4_address
CAF_CORE_EXPORT std::string to_string(const ipv4_address& x);

/// Tries to parse the content of `str` into `dest`.
/// @relates ipv4_address
CAF_CORE_EXPORT error parse(string_view str, ipv4_address& dest);

} // namespace caf
