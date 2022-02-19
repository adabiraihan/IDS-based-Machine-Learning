#pragma once

#include <utility>
#include <unordered_map>

#include <caf/actor.hpp>
#include <caf/variant.hpp>
#include <caf/optional.hpp>

#include "broker/data.hh"
#include "broker/fwd.hh"
#include "broker/publisher_id.hh"
#include "broker/time.hh"

namespace broker {

/// Sets a value in the key-value store.
struct put_command {
  data key;
  data value;
  caf::optional<timespan> expiry;
  publisher_id publisher;
};

template <class Inspector>
bool inspect(Inspector& f, put_command& x) {
  return f.object(x).fields(f.field("key", x.key), f.field("value", x.value),
                            f.field("expiry", x.expiry),
                            f.field("publisher", x.publisher));
}

/// Sets a value in the key-value store if its key does not already exist.
struct put_unique_command {
  data key;
  data value;
  caf::optional<timespan> expiry;
  caf::actor who;
  request_id req_id;
  publisher_id publisher;
};

template <class Inspector>
bool inspect(Inspector& f, put_unique_command& x) {
  return f.object(x).fields(f.field("key", x.key), f.field("value", x.value),
                            f.field("expiry", x.expiry), f.field("who", x.who),
                            f.field("req_id", x.req_id),
                            f.field("publisher", x.publisher));
}

/// Removes a value in the key-value store.
struct erase_command {
  data key;
  publisher_id publisher;
};

template <class Inspector>
bool inspect(Inspector& f, erase_command& x) {
  return f.object(x).fields(f.field("key", x.key),
                            f.field("publisher", x.publisher));
}

/// Removes a value in the key-value store as a result of an expiration. The
/// master sends this message type to the clones in order to allow them to
/// differentiate between a user actively removing an entry versus the master
/// removing it after expiration.
struct expire_command {
  data key;
  publisher_id publisher;
};

template <class Inspector>
bool inspect(Inspector& f, expire_command& x) {
  return f.object(x).fields(f.field("key", x.key),
                            f.field("publisher", x.publisher));
}

/// Adds a value to the existing value.
struct add_command {
  data key;
  data value;
  data::type init_type;
  caf::optional<timespan> expiry;
  publisher_id publisher;
};

template <class Inspector>
bool inspect(Inspector& f, add_command& x) {
  return f.object(x).fields(f.field("key", x.key), f.field("value", x.value),
                            f.field("init_type", x.init_type),
                            f.field("expiry", x.expiry),
                            f.field("publisher", x.publisher));
}

/// Subtracts a value to the existing value.
struct subtract_command {
  data key;
  data value;
  caf::optional<timespan> expiry;
  publisher_id publisher;
};

template <class Inspector>
bool inspect(Inspector& f, subtract_command& x) {
  return f.object(x).fields(f.field("key", x.key), f.field("value", x.value),
                            f.field("expiry", x.expiry),
                            f.field("publisher", x.publisher));
}

/// Causes the master to reply with a snapshot of its state.
struct snapshot_command {
  caf::actor remote_core;
  caf::actor remote_clone;
};

template <class Inspector>
bool inspect(Inspector& f, snapshot_command& x) {
  return f.object(x).fields(f.field("remote_core", x.remote_core),
                            f.field("remote_clone", x.remote_clone));
}

/// Since snapshots are sent to clones on a different channel, this allows
/// clones to coordinate the reception of snapshots with the stream of
/// updates that the master may have independently made to it.
struct snapshot_sync_command {
  caf::actor remote_clone;
};

template <class Inspector>
bool inspect(Inspector& f, snapshot_sync_command& x) {
  return f.object(x).fields(f.field("remote_clone", x.remote_clone));
}

/// Sets the full state of all receiving replicates to the included snapshot.
struct set_command {
  std::unordered_map<data, data> state;
};

template <class Inspector>
bool inspect(Inspector& f, set_command& x) {
  return f.object(x).fields(f.field("state", x.state));
}

/// Drops all values.
struct clear_command {
  publisher_id publisher;
};

template <class Inspector>
bool inspect(Inspector& f, clear_command& x) {
  return f.object(x).fields();
}

class internal_command {
public:
  enum class type : uint8_t {
    none,
    put_command,
    put_unique_command,
    erase_command,
    expire_command,
    add_command,
    subtract_command,
    snapshot_command,
    snapshot_sync_command,
    set_command,
    clear_command,
  };

  using variant_type
    = caf::variant<none, put_command, put_unique_command, erase_command,
                   expire_command, add_command, subtract_command,
                   snapshot_command, snapshot_sync_command, set_command,
                   clear_command>;

  variant_type content;

  internal_command(variant_type value);

  internal_command() = default;
  internal_command(internal_command&&) = default;
  internal_command(const internal_command&) = default;
  internal_command& operator=(internal_command&&) = default;
  internal_command& operator=(const internal_command&) = default;
};

template <class T, class... Ts>
internal_command make_internal_command(Ts&&... xs) {
  return internal_command{T{std::forward<Ts>(xs)...}};
}

template <class Inspector>
bool inspect(Inspector& f, internal_command& x) {
  return f.object(x).fields(f.field("content", x.content));
}

namespace detail {

template <internal_command::type Value>
using internal_command_tag_token
  = std::integral_constant<internal_command::type, Value>;

template <class T>
struct internal_command_tag_oracle;

#define INTERNAL_COMMAND_TAG_ORACLE(type_name)                                 \
  template <>                                                                  \
  struct internal_command_tag_oracle<type_name>                                \
    : internal_command_tag_token<internal_command::type::type_name> {}

INTERNAL_COMMAND_TAG_ORACLE(none);
INTERNAL_COMMAND_TAG_ORACLE(put_command);
INTERNAL_COMMAND_TAG_ORACLE(put_unique_command);
INTERNAL_COMMAND_TAG_ORACLE(erase_command);
INTERNAL_COMMAND_TAG_ORACLE(expire_command);
INTERNAL_COMMAND_TAG_ORACLE(add_command);
INTERNAL_COMMAND_TAG_ORACLE(subtract_command);
INTERNAL_COMMAND_TAG_ORACLE(snapshot_command);
INTERNAL_COMMAND_TAG_ORACLE(snapshot_sync_command);
INTERNAL_COMMAND_TAG_ORACLE(set_command);
INTERNAL_COMMAND_TAG_ORACLE(clear_command);

#undef INTERNAL_COMMAND_TAG_ORACLE

} // namespace detail

/// Returns the `internal_command::type` tag for `T`.
/// @relates internal_internal_command
template <class T>
constexpr internal_command::type internal_command_tag() {
  return detail::internal_command_tag_oracle<T>::value;
}

/// Returns the `internal_command::type` tag for `T` as `uint8_t`.
/// @relates internal_internal_command
template <class T>
constexpr uint8_t internal_command_uint_tag() {
  return static_cast<uint8_t>(detail::internal_command_tag_oracle<T>::value);
}

} // namespace broker
