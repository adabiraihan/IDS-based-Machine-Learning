// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <type_traits>

#include "caf/actor_traits.hpp"
#include "caf/catch_all.hpp"
#include "caf/message_id.hpp"
#include "caf/none.hpp"
#include "caf/sec.hpp"
#include "caf/system_messages.hpp"
#include "caf/typed_behavior.hpp"

#include "caf/detail/type_list.hpp"
#include "caf/detail/typed_actor_util.hpp"

namespace caf {

/// This helper class identifies an expected response message and enables
/// `request(...).then(...)`.
template <class ActorType, class Policy>
class response_handle {
public:
  // -- member types -----------------------------------------------------------

  using actor_type = ActorType;

  using traits = actor_traits<actor_type>;

  using policy_type = Policy;

  using response_type = typename policy_type::response_type;

  // -- constructors, destructors, and assignment operators --------------------

  response_handle() = delete;

  response_handle(const response_handle&) = delete;

  response_handle& operator=(const response_handle&) = delete;

  response_handle(response_handle&&) noexcept = default;

  response_handle& operator=(response_handle&&) noexcept = delete;

  template <class... Ts>
  explicit response_handle(actor_type* self, Ts&&... xs)
    : self_(self), policy_(std::forward<Ts>(xs)...) {
    // nop
  }

  // -- non-blocking API -------------------------------------------------------

  template <class T = traits, class F, class OnError>
  detail::enable_if_t<T::is_non_blocking> await(F f, OnError g) const {
    static_assert(detail::has_add_awaited_response_handler_v<ActorType>,
                  "this actor type does not support awaiting responses, "
                  "try using .then instead");
    static_assert(detail::is_callable<F>::value,
                  "F must provide a single, non-template operator()");
    static_assert(detail::is_callable_with<OnError, error&>::value,
                  "OnError must provide an operator() that takes a caf::error");
    using result_type = typename detail::get_callable_trait<F>::result_type;
    static_assert(std::is_same<void, result_type>::value,
                  "response handlers are not allowed to have a return "
                  "type other than void");
    policy_type::template type_checker<F>::check();
    policy_.await(self_, std::move(f), std::move(g));
  }

  template <class T = traits, class F>
  detail::enable_if_t<detail::has_call_error_handler_v<ActorType> //
                      && T::is_non_blocking>
  await(F f) const {
    auto self = self_;
    await(std::move(f), [self](error& err) { self->call_error_handler(err); });
  }

  template <class T = traits, class F, class OnError>
  detail::enable_if_t<T::is_non_blocking> then(F f, OnError g) const {
    static_assert(detail::has_add_multiplexed_response_handler_v<ActorType>,
                  "this actor type does not support multiplexed responses, "
                  "try using .await instead");
    static_assert(detail::is_callable<F>::value,
                  "F must provide a single, non-template operator()");
    static_assert(detail::is_callable_with<OnError, error&>::value,
                  "OnError must provide an operator() that takes a caf::error");
    using result_type = typename detail::get_callable_trait<F>::result_type;
    static_assert(std::is_same<void, result_type>::value,
                  "response handlers are not allowed to have a return "
                  "type other than void");
    policy_type::template type_checker<F>::check();
    policy_.then(self_, std::move(f), std::move(g));
  }

  template <class T = traits, class F>
  detail::enable_if_t<detail::has_call_error_handler_v<ActorType> //
                      && T::is_non_blocking>
  then(F f) const {
    auto self = self_;
    then(std::move(f), [self](error& err) { self->call_error_handler(err); });
  }

  // -- blocking API -----------------------------------------------------------

  template <class T = traits, class F = none_t, class OnError = none_t,
            class = detail::enable_if_t<T::is_blocking>>
  detail::is_handler_for_ef<OnError, error> receive(F f, OnError g) {
    static_assert(detail::is_callable<F>::value,
                  "F must provide a single, non-template operator()");
    static_assert(detail::is_callable_with<OnError, error&>::value,
                  "OnError must provide an operator() that takes a caf::error");
    using result_type = typename detail::get_callable_trait<F>::result_type;
    static_assert(std::is_same<void, result_type>::value,
                  "response handlers are not allowed to have a return "
                  "type other than void");
    policy_type::template type_checker<F>::check();
    policy_.receive(self_, std::move(f), std::move(g));
  }

  template <class T = traits, class OnError = none_t, class F = none_t,
            class = detail::enable_if_t<T::is_blocking>>
  detail::is_handler_for_ef<OnError, error> receive(OnError g, F f) {
    // TODO: allowing blocking actors to pass the error handler in first may be
    //       more flexible, but it makes the API asymmetric. Consider
    //       deprecating / removing this member function.
    receive(std::move(f), std::move(g));
  }

  template <class T = policy_type, class OnError = none_t, class F = none_t,
            class E = detail::is_handler_for_ef<OnError, error>,
            class = detail::enable_if_t<T::is_trivial>>
  void receive(OnError g, catch_all<F> f) {
    // TODO: this bypasses the policy. Either we deprecate `catch_all` or *all*
    //       policies must support it. Currently, we only enable this member
    //       function on the trivial policy for backwards compatibility.
    typename actor_type::accept_one_cond rc;
    self_->varargs_receive(rc, id(), std::move(g), std::move(f));
  }

  // -- properties -------------------------------------------------------------

  template <class T = policy_type, class = detail::enable_if_t<T::is_trivial>>
  message_id id() const noexcept {
    return policy_.id();
  }

  actor_type* self() noexcept {
    return self_;
  }

private:
  /// Points to the parent actor.
  actor_type* self_;

  /// Configures how the actor wants to process an incoming response.
  policy_type policy_;
};

} // namespace caf
