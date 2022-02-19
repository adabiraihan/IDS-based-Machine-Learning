#pragma once

#include <array>
#include <cstdint>
#include <string>

#include <caf/detail/comparable.hpp>
#include <caf/error.hpp>
#include <caf/ip_address.hpp>
#include <caf/string_view.hpp>

namespace broker {

/// Stores an IPv4 or IPv6 address.
class address : caf::detail::comparable<address> {
public:
  /// Distinguishes between address types.
  enum class family : uint8_t {
    ipv4,
    ipv6,
  };

  /// Distinguishes between address byte ordering.
  enum class byte_order : uint8_t {
    host,
    network,
  };

  /// Default construct an invalid address.
  address() = default;

  /// Construct an address from raw bytes.
  /// @param bytes A pointer to the raw representation.  This must point
  /// to 4 bytes if *fam* is `family::ipv4` and 16 bytes for `family::ipv6`.
  /// @param fam The type of address.
  /// @param order The byte order in which *bytes* is stored.
  address(const uint32_t* bytes, family fam, byte_order order);

  /// Mask out lower bits of the address.
  /// @param top_bits_to_keep The number of bits to *not* mask out, counting
  /// from the highest order bit.  The value is always interpreted relative to
  /// the IPv6 bit width, even if the address is IPv4.  That means to compute
  /// 192.168.1.2/16, pass in 112 (i.e. 96 + 16).  The value must range from
  /// 0 to 128.
  /// @returns true on success.
  bool mask(uint8_t top_bits_to_keep);

  /// @returns true if the address is IPv4.
  bool is_v4() const noexcept {
    return addr_.embeds_v4();
  }

  /// @returns true if the address is IPv6.
  bool is_v6() const noexcept {
    return !is_v4();
  }

  /// @returns the raw bytes of the address in network order. For IPv4
  /// addresses, this uses the IPv4-mapped IPv6 address representation.
  std::array<uint8_t, 16>& bytes() {
    return addr_.bytes();
  }

  /// @returns the raw bytes of the address in network order. For IPv4
  /// addresses, this uses the IPv4-mapped IPv6 address representation.
  const std::array<uint8_t, 16>& bytes() const {
    return addr_.bytes();
  }

  auto compare(const address& other) const noexcept {
    return addr_.compare(other.addr_);
  }

  size_t hash() const;

  template <class Inspector>
  friend bool inspect(Inspector& f, address& x) {
    // We transparently expose the member variable. Hence, broker::address and
    // caf::ip_address are one and the same to CAF inspectors.
    return inspect(f, x.addr_);
  }

  friend bool convert(const address& a, std::string& str) {
    str = to_string(a.addr_);
    return true;
  }

  friend bool convert(const std::string& str, address& a) {
    if (auto err = parse(str, a.addr_))
      return false;
    return true;
  }

private:
  caf::ip_address addr_;
};

/// @relates address
bool convert(const std::string& str, address& a);

/// @relates address
bool convert(const address& a, std::string& str);

} // namespace broker

namespace std {

/// @relates address
template <>
struct hash<broker::address> {
  size_t operator()(const broker::address& x) const {
    return x.hash();
  }
};

} // namespace std
