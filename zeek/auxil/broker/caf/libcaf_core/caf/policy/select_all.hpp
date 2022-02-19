// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <vector>

#include "caf/behavior.hpp"
#include "caf/config.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/detail/typed_actor_util.hpp"
#include "caf/error.hpp"
#include "caf/logger.hpp"
#include "caf/message_id.hpp"

namespace caf::detail {

template <class F, class T>
struct select_all_helper {
  std::vector<T> results;
  std::shared_ptr<size_t> pending;
  F f;

  void operator()(T& x) {
    CAF_LOG_TRACE(CAF_ARG2("pending", *pending));
    if (*pending > 0) {
      results.emplace_back(std::move(x));
      if (--*pending == 0)
        f(std::move(results));
    }
  }

  template <class Fun>
  select_all_helper(size_t pending, Fun&& f)
    : pending(std::make_shared<size_t>(pending)), f(std::forward<Fun>(f)) {
    results.reserve(pending);
  }

  auto wrap() {
    return [this](T& x) { (*this)(x); };
  }
};

template <class F, class... Ts>
struct select_all_tuple_helper {
  using value_type = std::tuple<Ts...>;
  std::vector<value_type> results;
  std::shared_ptr<size_t> pending;
  F f;

  void operator()(Ts&... xs) {
    CAF_LOG_TRACE(CAF_ARG2("pending", *pending));
    if (*pending > 0) {
      results.emplace_back(std::move(xs)...);
      if (--*pending == 0)
        f(std::move(results));
    }
  }

  template <class Fun>
  select_all_tuple_helper(size_t pending, Fun&& f)
    : pending(std::make_shared<size_t>(pending)), f(std::forward<Fun>(f)) {
    results.reserve(pending);
  }

  auto wrap() {
    return [this](Ts&... xs) { (*this)(xs...); };
  }
};

template <class F, class = typename detail::get_callable_trait<F>::arg_types>
struct select_select_all_helper;

template <class F, class... Ts>
struct select_select_all_helper<
  F, detail::type_list<std::vector<std::tuple<Ts...>>>> {
  using type = select_all_tuple_helper<F, Ts...>;
};

template <class F, class T>
struct select_select_all_helper<F, detail::type_list<std::vector<T>>> {
  using type = select_all_helper<F, T>;
};

template <class F>
using select_all_helper_t = typename select_select_all_helper<F>::type;

} // namespace caf::detail

namespace caf::policy {

/// Enables a `response_handle` to fan-in all responses messages into a single
/// result (a `vector` that stores all received results).
/// @relates mixin::requester
/// @relates response_handle
template <class ResponseType>
class select_all {
public:
  static constexpr bool is_trivial = false;

  using response_type = ResponseType;

  using message_id_list = std::vector<message_id>;

  template <class Fun>
  using type_checker
    = detail::type_checker<response_type,
                           detail::select_all_helper_t<detail::decay_t<Fun>>>;

  explicit select_all(message_id_list ids) : ids_(std::move(ids)) {
    CAF_ASSERT(ids_.size()
               <= static_cast<size_t>(std::numeric_limits<int>::max()));
  }

  select_all(select_all&&) noexcept = default;

  select_all& operator=(select_all&&) noexcept = default;

  template <class Self, class F, class OnError>
  void await(Self* self, F&& f, OnError&& g) const {
    CAF_LOG_TRACE(CAF_ARG(ids_));
    auto bhvr = make_behavior(std::forward<F>(f), std::forward<OnError>(g));
    for (auto id : ids_)
      self->add_awaited_response_handler(id, bhvr);
  }

  template <class Self, class F, class OnError>
  void then(Self* self, F&& f, OnError&& g) const {
    CAF_LOG_TRACE(CAF_ARG(ids_));
    auto bhvr = make_behavior(std::forward<F>(f), std::forward<OnError>(g));
    for (auto id : ids_)
      self->add_multiplexed_response_handler(id, bhvr);
  }

  template <class Self, class F, class G>
  void receive(Self* self, F&& f, G&& g) const {
    CAF_LOG_TRACE(CAF_ARG(ids_));
    using helper_type = detail::select_all_helper_t<detail::decay_t<F>>;
    helper_type helper{ids_.size(), std::forward<F>(f)};
    auto error_handler = [&](error& err) {
      if (*helper.pending > 0) {
        *helper.pending = 0;
        helper.results.clear();
        g(err);
      }
    };
    for (auto id : ids_) {
      typename Self::accept_one_cond rc;
      auto error_handler_copy = error_handler;
      self->varargs_receive(rc, id, helper.wrap(), error_handler_copy);
    }
  }

  const message_id_list& ids() const noexcept {
    return ids_;
  }

private:
  template <class F, class OnError>
  behavior make_behavior(F&& f, OnError&& g) const {
    using namespace detail;
    using helper_type = select_all_helper_t<decay_t<F>>;
    helper_type helper{ids_.size(), std::move(f)};
    auto pending = helper.pending;
    auto error_handler = [pending{std::move(pending)},
                          g{std::forward<OnError>(g)}](error& err) mutable {
      CAF_LOG_TRACE(CAF_ARG2("pending", *pending));
      if (*pending > 0) {
        *pending = 0;
        g(err);
      }
    };
    return {
      std::move(helper),
      std::move(error_handler),
    };
  }

  message_id_list ids_;
};

} // namespace caf::policy
