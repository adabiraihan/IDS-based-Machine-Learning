// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

#include "caf/abstract_channel.hpp"
#include "caf/attachable.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/functor_attachable.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/execution_unit.hpp"
#include "caf/exit_reason.hpp"
#include "caf/fwd.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/message_id.hpp"
#include "caf/node_id.hpp"

namespace caf {

/// A unique actor ID.
/// @relates abstract_actor
using actor_id = uint64_t;

/// Denotes an ID that is never used by an actor.
constexpr actor_id invalid_actor_id = 0;

/// Base class for all actor implementations.
class CAF_CORE_EXPORT abstract_actor : public abstract_channel {
public:
  actor_control_block* ctrl() const;

  ~abstract_actor() override;

  abstract_actor(const abstract_actor&) = delete;
  abstract_actor& operator=(const abstract_actor&) = delete;

  /// Cleans up any remaining state before the destructor is called.
  /// This function makes sure it is safe to call virtual functions
  /// in sub classes before destroying the object, because calling
  /// virtual function in the destructor itself is not safe. Any override
  /// implementation is required to call `super::destroy()` at the end.
  virtual void on_destroy();

  void enqueue(strong_actor_ptr sender, message_id mid, message msg,
               execution_unit* host) override;

  /// Enqueues a new message wrapped in a `mailbox_element` to the actor.
  /// This `enqueue` variant allows to define forwarding chains.
  virtual void enqueue(mailbox_element_ptr what, execution_unit* host) = 0;

  /// Attaches `ptr` to this actor. The actor will call `ptr->detach(...)` on
  /// exit, or immediately if it already finished execution.
  virtual void attach(attachable_ptr ptr) = 0;

  /// Convenience function that attaches the functor `f` to this actor. The
  /// actor executes `f()` on exit or immediately if it is not running.
  template <class F>
  void attach_functor(F f) {
    attach(attachable_ptr{new detail::functor_attachable<F>(std::move(f))});
  }

  /// Returns the logical actor address.
  actor_addr address() const noexcept;

  /// Detaches the first attached object that matches `what`.
  virtual size_t detach(const attachable::token& what) = 0;

  /// Returns the set of accepted messages types as strings or
  /// an empty set if this actor is untyped.
  virtual std::set<std::string> message_types() const;

  /// Returns the ID of this actor.
  actor_id id() const noexcept;

  /// Returns the node this actor is living on.
  node_id node() const noexcept;

  /// Returns the system that created this actor (or proxy).
  actor_system& home_system() const noexcept;

  /****************************************************************************
   *                 here be dragons: end of public interface                 *
   ****************************************************************************/

  /// @cond PRIVATE

  /// Called by the testing DSL to peek at the next element in the mailbox. Do
  /// not call this function in production code! The default implementation
  /// always returns `nullptr`.
  /// @returns A pointer to the next mailbox element or `nullptr` if the
  ///          mailbox is empty or the actor does not have a mailbox.
  virtual mailbox_element* peek_at_next_mailbox_element();

  template <class... Ts>
  void eq_impl(message_id mid, strong_actor_ptr sender, execution_unit* ctx,
               Ts&&... xs) {
    enqueue(make_mailbox_element(std::move(sender), mid, {},
                                 std::forward<Ts>(xs)...),
            ctx);
  }

  // flags storing runtime information                      used by ...
  static constexpr int has_timeout_flag = 0x0004;      // single_timeout
  static constexpr int is_registered_flag = 0x0008;    // (several actors)
  static constexpr int is_initialized_flag = 0x0010;   // event-based actors
  static constexpr int is_blocking_flag = 0x0020;      // blocking_actor
  static constexpr int is_detached_flag = 0x0040;      // local_actor
  static constexpr int collects_metrics_flag = 0x0080; // local_actor
  static constexpr int is_serializable_flag = 0x0100;  // local_actor
  static constexpr int is_migrated_from_flag = 0x0200; // local_actor
  static constexpr int has_used_aout_flag = 0x0400;    // local_actor
  static constexpr int is_terminated_flag = 0x0800;    // local_actor
  static constexpr int is_cleaned_up_flag = 0x1000;    // monitorable_actor
  static constexpr int is_shutting_down_flag = 0x2000; // scheduled_actor

  void setf(int flag) {
    auto x = flags();
    flags(x | flag);
  }

  void unsetf(int flag) {
    auto x = flags();
    flags(x & ~flag);
  }

  bool getf(int flag) const {
    return (flags() & flag) != 0;
  }

  /// Sets `is_registered_flag` and calls `system().registry().inc_running()`.
  void register_at_system();

  /// Unsets `is_registered_flag` and calls `system().registry().dec_running()`.
  void unregister_from_system();

  /// Causes the actor to establish a link to `other`.
  virtual void add_link(abstract_actor* other) = 0;

  /// Causes the actor to remove any established link to `other`.
  virtual void remove_link(abstract_actor* other) = 0;

  /// Adds an entry to `other` to the link table of this actor.
  /// @warning Must be called inside a critical section, i.e.,
  ///          while holding `mtx_`.
  virtual bool add_backlink(abstract_actor* other) = 0;

  /// Removes an entry to `other` from the link table of this actor.
  /// @warning Must be called inside a critical section, i.e.,
  ///          while holding `mtx_`.
  virtual bool remove_backlink(abstract_actor* other) = 0;

  /// Calls `fun` with exclusive access to an actor's state.
  template <class F>
  auto exclusive_critical_section(F fun) -> decltype(fun()) {
    std::unique_lock<std::mutex> guard{mtx_};
    return fun();
  }

  /// Calls `fun` with readonly access to an actor's state.
  template <class F>
  auto shared_critical_section(F fun) -> decltype(fun()) {
    std::unique_lock<std::mutex> guard{mtx_};
    return fun();
  }

  /// Calls `fun` with exclusive access to the state of both `p1` and `p2`. This
  /// function guarantees that the order of acquiring the locks is always
  /// identical, independently from the order of `p1` and `p2`.
  template <class F>
  static auto joined_exclusive_critical_section(abstract_actor* p1,
                                                abstract_actor* p2, F fun)
    -> decltype(fun()) {
    // Make sure to allocate locks always in the same order by starting on the
    // actor with the lowest address.
    CAF_ASSERT(p1 != p2 && p1 != nullptr && p2 != nullptr);
    if (p1 < p2) {
      std::unique_lock<std::mutex> guard1{p1->mtx_};
      std::unique_lock<std::mutex> guard2{p2->mtx_};
      return fun();
    }
    std::unique_lock<std::mutex> guard1{p2->mtx_};
    std::unique_lock<std::mutex> guard2{p1->mtx_};
    return fun();
  }

  /// @endcond

protected:
  explicit abstract_actor(actor_config& cfg);

  // Guards potentially concurrent access to the state. For example,
  // `exit_state_`, `attachables_`, and `links_` in a `monitorable_actor`.
  mutable std::mutex mtx_;
};

} // namespace caf
