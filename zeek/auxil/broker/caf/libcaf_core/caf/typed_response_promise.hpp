// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <type_traits>

#include "caf/detail/type_list.hpp"
#include "caf/make_message.hpp"
#include "caf/response_promise.hpp"

namespace caf {

/// Enables statically typed actors to delay a response message by capturing the
/// context of a request message. This is particularly useful when an actor
/// needs to communicate to other actors in order to fulfill a request for a
/// client.
template <class... Ts>
class typed_response_promise {
public:
  // -- friends ----------------------------------------------------------------

  friend class local_actor;

  // -- member types -----------------------------------------------------------

  using forwarding_stack = response_promise::forwarding_stack;

  // -- constructors, destructors, and assignment operators --------------------

  typed_response_promise() = default;

  typed_response_promise(typed_response_promise&&) = default;

  typed_response_promise(const typed_response_promise&) = default;

  typed_response_promise& operator=(typed_response_promise&&) = default;

  typed_response_promise& operator=(const typed_response_promise&) = default;

  [[deprecated("use the default constructor instead")]] //
  typed_response_promise(none_t x)
    : promise_(x) {
    // nop
  }

  // -- properties -------------------------------------------------------------

  /// @copydoc response_promise::async
  bool async() const {
    return promise_.async();
  }

  /// @copydoc response_promise::pending
  bool pending() const {
    return promise_.pending();
  }

  /// @copydoc response_promise::source
  strong_actor_ptr source() const {
    return promise_.source();
  }

  /// @copydoc response_promise::stages
  forwarding_stack stages() const {
    return promise_.stages();
  }

  /// @copydoc response_promise::next
  strong_actor_ptr next() const {
    return promise_.next();
  }

  /// @copydoc response_promise::id
  message_id id() const {
    return promise_.id();
  }

  [[deprecated("use the typed_response_promise directly")]]
  operator response_promise&() {
    return promise_;
  }

  // -- delivery ---------------------------------------------------------------

  /// Satisfies the promise by sending a non-error response message.
  template <class... Us>
  std::enable_if_t<(std::is_constructible<Ts, Us>::value && ...)>
  deliver(Us... xs) {
    promise_.deliver(Ts{std::forward<Us>(xs)}...);
  }

  /// Satisfies the promise by sending an empty response message.
  template <class L = detail::type_list<Ts...>>
  std::enable_if_t<std::is_same<L, detail::type_list<void>>::value> deliver() {
    promise_.deliver();
  }

  /// Satisfies the promise by sending an error response message.
  /// For non-requests, nothing is done.
  void deliver(error x) {
    promise_.deliver(std::move(x));
  }

  /// Satisfies the promise by sending either an error or a non-error response
  /// message.
  template <class T>
  std::enable_if_t<
    std::is_same<detail::type_list<T>, detail::type_list<Ts...>>::value>
  deliver(expected<T> x) {
    promise_.deliver(std::move(x));
  }

  // -- delegation -------------------------------------------------------------

  /// Satisfies the promise by delegating to another actor.
  template <message_priority P = message_priority::normal, class Handle = actor,
            class... Us>
  auto delegate(const Handle& dest, Us&&... xs) {
    return promise_.template delegate<P>(dest, std::forward<Us>(xs)...);
  }

private:
  // -- constructors that are visible only to friends --------------------------

  typed_response_promise(local_actor* self, strong_actor_ptr source,
                         forwarding_stack stages, message_id id)
    : promise_(self, std::move(source), std::move(stages), id) {
    // nop
  }

  typed_response_promise(local_actor* self, mailbox_element& src)
    : promise_(self, src) {
    // nop
  }

  // -- member variables -------------------------------------------------------

  response_promise promise_;
};

} // namespace caf
