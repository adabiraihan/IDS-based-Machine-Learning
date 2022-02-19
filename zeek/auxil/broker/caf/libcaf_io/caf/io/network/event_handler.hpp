// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/io_export.hpp"
#include "caf/io/fwd.hpp"
#include "caf/io/network/native_socket.hpp"
#include "caf/io/network/operation.hpp"
#include "caf/io/receive_policy.hpp"

namespace caf::io::network {

/// A socket I/O event handler.
class CAF_IO_EXPORT event_handler {
public:
  /// Stores various status flags and user-defined config parameters.
  struct state {
    /// Stores whether the socket is currently registered for reading.
    bool reading : 1;

    /// Stores whether the socket is currently registered for writing.
    bool writing : 1;

    /// Stores whether the parent actor demanded write receipts.
    bool ack_writes : 1;

    /// Stores whether graceful_shutdown() was called.
    bool shutting_down : 1;

    /// Stores what receive policy is currently active.
    unsigned rd_flag : 2;
  };

  event_handler(default_multiplexer& dm, native_socket sockfd);

  virtual ~event_handler();

  /// Returns true once the requested operation is done, i.e.,
  /// to signalize the multiplexer to remove this handler.
  /// The handler remains in the event loop as long as it returns false.
  virtual void handle_event(operation op) = 0;

  /// Callback to signalize that this handler has been removed
  /// from the event loop for operations of type `op`.
  virtual void removed_from_loop(operation op) = 0;

  /// Shuts down communication on the managed socket, eventually removing
  /// this event handler from the I/O loop.
  virtual void graceful_shutdown() = 0;

  /// Returns the native socket handle for this handler.
  native_socket fd() const {
    return fd_;
  }

  /// Returns the `multiplexer` this acceptor belongs to.
  default_multiplexer& backend() {
    return backend_;
  }

  /// Returns the bit field storing the subscribed events.
  int eventbf() const {
    return eventbf_;
  }

  /// Sets the bit field storing the subscribed events.
  void eventbf(int value) {
    eventbf_ = value;
  }

  /// Checks whether `close_read_channel` has been called.
  bool read_channel_closed() const {
    return !state_.reading;
  }

  /// Removes the file descriptor from the event loop of the parent.
  void passivate();

  /// Returns whether this event handlers signals successful writes to its
  /// parent actor.
  bool ack_writes() {
    return state_.ack_writes;
  }

  /// Sets whether this event handlers signals successful writes to its parent
  /// actor.
  void ack_writes(bool x) {
    state_.ack_writes = x;
  }

protected:
  /// Adds the file descriptor to the event loop of the parent.
  void activate();

  /// Sets flags for asynchronous event handling on the socket handle.
  void set_fd_flags();

  native_socket fd_;
  state state_;
  int eventbf_;
  default_multiplexer& backend_;
};

} // namespace caf::io::network
