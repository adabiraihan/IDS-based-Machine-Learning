// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include "caf/actor_addr.hpp"
#include "caf/actor_control_block.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/message.hpp"
#include "caf/stream_priority.hpp"
#include "caf/stream_slot.hpp"
#include "caf/tag/boxing_type.hpp"
#include "caf/variant.hpp"

namespace caf {

/// Transmits stream data.
struct downstream_msg_batch {
  /// Allows the testing DSL to unbox this type automagically.
  using outer_type = downstream_msg;

  /// Size of the type-erased vector<T> (used credit).
  int32_t xs_size;

  /// A type-erased vector<T> containing the elements of the batch.
  message xs;

  /// ID of this batch (ascending numbering).
  int64_t id;
};

/// Orderly shuts down a stream after receiving an ACK for the last batch.
struct downstream_msg_close {
  /// Allows the testing DSL to unbox this type automagically.
  using outer_type = downstream_msg;
};

/// Propagates a fatal error from sources to sinks.
struct downstream_msg_forced_close {
  /// Allows the testing DSL to unbox this type automagically.
  using outer_type = downstream_msg;

  /// Reason for shutting down the stream.
  error reason;
};

/// Stream messages that travel downstream, i.e., batches and close messages.
struct CAF_CORE_EXPORT downstream_msg : tag::boxing_type {
  // -- nested types -----------------------------------------------------------

  using batch = downstream_msg_batch;

  using close = downstream_msg_close;

  using forced_close = downstream_msg_forced_close;

  // -- member types -----------------------------------------------------------

  /// Lists all possible options for the payload.
  using alternatives = detail::type_list<batch, close, forced_close>;

  /// Stores one of `alternatives`.
  using content_type = variant<batch, close, forced_close>;

  // -- constructors, destructors, and assignment operators --------------------

  template <class T>
  downstream_msg(stream_slots s, actor_addr addr, T&& x)
    : slots(s), sender(std::move(addr)), content(std::forward<T>(x)) {
    // nop
  }

  downstream_msg() = default;
  downstream_msg(downstream_msg&&) = default;
  downstream_msg(const downstream_msg&) = default;
  downstream_msg& operator=(downstream_msg&&) = default;
  downstream_msg& operator=(const downstream_msg&) = default;

  // -- member variables -------------------------------------------------------

  /// ID of the affected stream.
  stream_slots slots;

  /// Address of the sender. Identifies the up- or downstream actor sending
  /// this message. Note that abort messages can get send after `sender`
  /// already terminated. Hence, `current_sender()` can be `nullptr`, because
  /// no strong pointers can be formed any more and receiver would receive an
  /// anonymous message.
  actor_addr sender;

  /// Palyoad of the message.
  content_type content;
};

/// Allows the testing DSL to unbox `downstream_msg` automagically.
template <class T>
const T& get(const downstream_msg& x) {
  return get<T>(x.content);
}

/// Allows the testing DSL to check whether `downstream_msg` holds a `T`.
template <class T>
bool is(const downstream_msg& x) {
  return holds_alternative<T>(x.content);
}

/// @relates downstream_msg
template <class T, class... Ts>
detail::enable_if_tt<detail::tl_contains<downstream_msg::alternatives, T>,
                     downstream_msg>
make(stream_slots slots, actor_addr addr, Ts&&... xs) {
  return {slots, std::move(addr), T{std::forward<Ts>(xs)...}};
}

/// @relates downstream_msg::batch
template <class Inspector>
bool inspect(Inspector& f, downstream_msg::batch& x) {
  return f.object(x).pretty_name("batch").fields(f.field("size", x.xs_size),
                                                 f.field("xs", x.xs),
                                                 f.field("id", x.id));
}

/// @relates downstream_msg::close
template <class Inspector>
bool inspect(Inspector& f, downstream_msg::close& x) {
  return f.object(x).pretty_name("close").fields();
}

/// @relates downstream_msg::forced_close
template <class Inspector>
bool inspect(Inspector& f, downstream_msg::forced_close& x) {
  return f.object(x)
    .pretty_name("forced_close")
    .fields(f.field("reason", x.reason));
}

/// @relates downstream_msg
template <class Inspector>
bool inspect(Inspector& f, downstream_msg& x) {
  return f.object(x)
    .pretty_name("downstream_msg")
    .fields(f.field("slots", x.slots), f.field("sender", x.sender),
            f.field("content", x.content));
}

} // namespace caf
