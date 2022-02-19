// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/actor_control_block.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/detail/io_export.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/io/fwd.hpp"
#include "caf/io/network/operation.hpp"
#include "caf/message.hpp"
#include "caf/ref_counted.hpp"

namespace caf::io::network {

/// A manager configures an I/O device and provides callbacks
/// for various I/O operations.
class CAF_IO_EXPORT manager : public ref_counted {
public:
  manager();

  ~manager() override;

  /// Sets the parent for this manager.
  /// @pre `parent() == nullptr`
  void set_parent(abstract_broker* ptr);

  /// Returns the parent broker of this manager.
  abstract_broker* parent();

  /// Returns `true` if this manager has a parent, `false` otherwise.
  bool detached() const {
    return !parent_;
  }

  /// Detach this manager from its parent and invoke `detach_message()``
  /// if `invoke_detach_message == true`.
  void detach(execution_unit* ctx, bool invoke_disconnect_message);

  /// Causes the manager to gracefully close its connection.
  virtual void graceful_shutdown() = 0;

  /// Removes the I/O device to the event loop of the middleman.
  virtual void remove_from_loop() = 0;

  /// Adds the I/O device to the event loop of the middleman.
  virtual void add_to_loop() = 0;

  /// Detaches this manager from its parent in case of an error.
  void io_failure(execution_unit* ctx, operation op);

protected:
  /// Creates a message signalizing a disconnect to the parent.
  virtual message detach_message() = 0;

  /// Detaches this manager from `ptr`.
  virtual void detach_from(abstract_broker* ptr) = 0;

  strong_actor_ptr parent_;
};

} // namespace caf::io::network
