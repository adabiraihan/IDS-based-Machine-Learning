// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <map>

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/intrusive/drr_queue.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/stream_slot.hpp"
#include "caf/unit.hpp"

namespace caf::policy {

/// Configures a dynamic WDRR queue for holding downstream messages.
class CAF_CORE_EXPORT downstream_messages {
public:
  // -- nested types -----------------------------------------------------------

  /// Configures a nested DRR queue.
  class CAF_CORE_EXPORT nested {
  public:
    // -- member types ---------------------------------------------------------

    using mapped_type = mailbox_element;

    using task_size_type = size_t;

    using deficit_type = size_t;

    using unique_pointer = mailbox_element_ptr;

    using handler_type = std::unique_ptr<inbound_path>;

    static task_size_type task_size(const downstream_msg_batch& x) noexcept;

    static constexpr task_size_type
    task_size(const downstream_msg_close&) noexcept {
      return 1;
    }

    static constexpr task_size_type
    task_size(const downstream_msg_forced_close&) noexcept {
      return 1;
    }

    static task_size_type task_size(const mailbox_element& x) noexcept;

    // -- constructors, destructors, and assignment operators ------------------

    template <class T>
    nested(T&& x) : handler(std::forward<T>(x)) {
      // nop
    }

    nested() = default;

    nested(nested&&) = default;

    nested& operator=(nested&&) = default;

    nested(const nested&) = delete;

    nested& operator=(const nested&) = delete;

    // -- member variables -----------------------------------------------------

    handler_type handler;

    size_t bulk_inserted_size = 0;
  };

  // -- member types -----------------------------------------------------------

  using mapped_type = mailbox_element;

  using task_size_type = size_t;

  using deficit_type = size_t;

  using pointer = mapped_type*;

  using unique_pointer = mailbox_element_ptr;

  using key_type = stream_slot;

  using nested_queue_type = intrusive::drr_queue<nested>;

  using queue_map_type = std::map<key_type, nested_queue_type>;

  // -- required functions for wdrr_dynamic_multiplexed_queue ------------------

  static key_type id_of(mailbox_element& x) noexcept;

  static bool enabled(const nested_queue_type& q) noexcept;

  static deficit_type quantum(const nested_queue_type& q,
                              deficit_type x) noexcept;

  // -- constructors, destructors, and assignment operators --------------------

  downstream_messages() = default;

  downstream_messages(const downstream_messages&) = default;

  downstream_messages& operator=(const downstream_messages&) = default;

  constexpr downstream_messages(unit_t) {
    // nop
  }

  // -- required functions for drr_queue ---------------------------------------

  static task_size_type task_size(const mailbox_element&) noexcept {
    return 1;
  }

  // -- required functions for wdrr_dynamic_multiplexed_queue ------------------

  static void cleanup(nested_queue_type&) noexcept;

  static bool push_back(nested_queue_type& sub_queue, pointer ptr) noexcept;

  static void lifo_append(nested_queue_type& sub_queue, pointer ptr) noexcept;

  static void stop_lifo_append(nested_queue_type& sub_queue) noexcept;
};

} // namespace caf::policy
