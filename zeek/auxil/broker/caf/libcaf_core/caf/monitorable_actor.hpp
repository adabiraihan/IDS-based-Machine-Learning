// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

#include "caf/abstract_actor.hpp"
#include "caf/actor_addr.hpp"
#include "caf/actor_cast.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/functor_attachable.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/system_messages.hpp"
#include "caf/typed_message_view.hpp"

namespace caf {

/// Base class for all actor implementations.
class CAF_CORE_EXPORT monitorable_actor : public abstract_actor {
public:
  /// Returns an implementation-dependent name for logging purposes, which
  /// is only valid as long as the actor is running. The default
  /// implementation simply returns "actor".
  virtual const char* name() const;

  void attach(attachable_ptr ptr) override;

  size_t detach(const attachable::token& what) override;

  // -- linking and monitoring -------------------------------------------------

  /// Links this actor to `x`.
  void link_to(const actor_addr& x) {
    auto ptr = actor_cast<strong_actor_ptr>(x);
    if (ptr && ptr->get() != this)
      add_link(ptr->get());
  }

  /// Links this actor to `x`.
  template <class ActorHandle>
  void link_to(const ActorHandle& x) {
    auto ptr = actor_cast<abstract_actor*>(x);
    if (ptr && ptr != this)
      add_link(ptr);
  }

  /// Unlinks this actor from `x`.
  void unlink_from(const actor_addr& x);

  /// Links this actor to `x`.
  template <class ActorHandle>
  void unlink_from(const ActorHandle& x) {
    auto ptr = actor_cast<abstract_actor*>(x);
    if (ptr && ptr != this)
      remove_link(ptr);
  }

  /// @cond PRIVATE

  /// Called by the runtime system to perform cleanup actions for this actor.
  /// Subtypes should always call this member function when overriding it.
  /// This member function is thread-safe, and if the actor has already exited
  /// upon invocation, nothing is done. The return value of this member
  /// function is ignored by scheduled actors.
  virtual bool cleanup(error&& reason, execution_unit* host);

  void add_link(abstract_actor* x) override;

  void remove_link(abstract_actor* x) override;

  bool add_backlink(abstract_actor* x) override;

  bool remove_backlink(abstract_actor* x) override;

  error fail_state() const;

  /// @endcond

protected:
  /// Allows subclasses to add additional cleanup code to the
  /// critical section in `cleanup`. This member function is
  /// called inside of a critical section.
  virtual void on_cleanup(const error& reason);

  /// Sends a response message if `what` is a request.
  void bounce(mailbox_element_ptr& what);

  /// Sends a response message if `what` is a request.
  void bounce(mailbox_element_ptr& what, const error& err);

  /// Creates a new actor instance.
  explicit monitorable_actor(actor_config& cfg);

  /****************************************************************************
   *                 here be dragons: end of public interface                 *
   ****************************************************************************/

  // precondition: `mtx_` is acquired
  void attach_impl(attachable_ptr& ptr) {
    ptr->next.swap(attachables_head_);
    attachables_head_.swap(ptr);
  }

  // precondition: `mtx_` is acquired
  size_t detach_impl(const attachable::token& what, bool stop_on_hit = false,
                     bool dry_run = false);

  // handles only `exit_msg` and `sys_atom` messages;
  // returns true if the message is handled
  bool handle_system_message(mailbox_element& x, execution_unit* ctx,
                             bool trap_exit);

  // handles `exit_msg`, `sys_atom` messages, and additionally `down_msg`
  // with `down_msg_handler`; returns true if the message is handled
  template <class F>
  bool handle_system_message(mailbox_element& x, execution_unit* context,
                             bool trap_exit, F& down_msg_handler) {
    if (auto view = make_typed_message_view<down_msg>(x.payload)) {
      down_msg_handler(get<0>(view));
      return true;
    }
    return handle_system_message(x, context, trap_exit);
  }

  // is protected by `mtx_` in actors that are not scheduled, but
  // can be accessed without lock for event-based and blocking actors
  error fail_state_;

  // only used in blocking and thread-mapped actors
  mutable std::condition_variable cv_;

  // attached functors that are executed on cleanup (monitors, links, etc)
  attachable_ptr attachables_head_;

  /// @endcond
};

} // namespace caf
