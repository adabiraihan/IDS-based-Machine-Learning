// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <type_traits>

#include "caf/all.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/config.hpp"
#include "caf/detail/gcd.hpp"
#include "caf/init_global_meta_objects.hpp"
#include "caf/test/unit_test.hpp"

CAF_PUSH_WARNINGS

/// The type of `_`.
struct wildcard {};

/// Allows ignoring individual messages elements in `expect` clauses, e.g.
/// `expect((int, int), from(foo).to(bar).with(1, _))`.
constexpr wildcard _ = wildcard{};

/// @relates wildcard
constexpr bool operator==(const wildcard&, const wildcard&) {
  return true;
}

template <size_t I, class T>
bool cmp_one(const caf::message& x, const T& y) {
  if (std::is_same<T, wildcard>::value)
    return true;
  return x.match_element<T>(I) && x.get_as<T>(I) == y;
}

template <size_t I, class... Ts>
typename std::enable_if<(I == sizeof...(Ts)), bool>::type
msg_cmp_rec(const caf::message&, const std::tuple<Ts...>&) {
  return true;
}

template <size_t I, class... Ts>
typename std::enable_if<(I < sizeof...(Ts)), bool>::type
msg_cmp_rec(const caf::message& x, const std::tuple<Ts...>& ys) {
  return cmp_one<I>(x, std::get<I>(ys)) && msg_cmp_rec<I + 1>(x, ys);
}

// allow comparing arbitrary `T`s to `message` objects for the purpose of the
// testing DSL
namespace caf {

template <class... Ts>
bool operator==(const message& x, const std::tuple<Ts...>& y) {
  return x.size() == sizeof...(Ts) && msg_cmp_rec<0>(x, y);
}

template <class T>
bool operator==(const message& x, const T& y) {
  return x.match_elements<T>() && x.get_as<T>(0) == y;
}

} // namespace caf

// dummy function to force ADL later on
// int inspect(int, int);

template <class T>
struct has_outer_type {
  template <class U>
  static auto sfinae(U* x) -> typename U::outer_type*;

  template <class U>
  static auto sfinae(...) -> std::false_type;

  using type = decltype(sfinae<T>(nullptr));
  static constexpr bool value = !std::is_same<type, std::false_type>::value;
};

// enables ADL in `with_content`
template <class T, class U>
T get(const has_outer_type<U>&);

// enables ADL in `with_content`
template <class T, class U>
bool is(const has_outer_type<U>&);

template <class Tup>
class elementwise_compare_inspector {
public:
  using result_type = bool;

  template <size_t X>
  using pos = std::integral_constant<size_t, X>;

  explicit elementwise_compare_inspector(const Tup& xs) : xs_(xs) {
    // nop
  }

  template <class... Ts>
  bool operator()(const Ts&... xs) {
    return iterate(pos<0>{}, xs...);
  }

private:
  template <size_t X>
  bool iterate(pos<X>) {
    // end of recursion
    return true;
  }

  template <size_t X, class T, class... Ts>
  typename std::enable_if<caf::meta::is_annotation<T>::value, bool>::type
  iterate(pos<X> pos, const T&, const Ts&... ys) {
    return iterate(pos, ys...);
  }

  template <size_t X, class T, class... Ts>
  typename std::enable_if<!caf::meta::is_annotation<T>::value, bool>::type
  iterate(pos<X>, const T& y, const Ts&... ys) {
    std::integral_constant<size_t, X + 1> next;
    check(y, get<X>(xs_));
    return iterate(next, ys...);
  }

  template <class T, class U>
  static void check(const T& x, const U& y) {
    CAF_CHECK_EQUAL(x, y);
  }

  template <class T>
  static void check(const T&, const wildcard&) {
    // nop
  }

  const Tup& xs_;
};

// -- unified access to all actor handles in CAF -------------------------------

/// Reduces any of CAF's handle types to an `abstract_actor` pointer.
class caf_handle : caf::detail::comparable<caf_handle>,
                   caf::detail::comparable<caf_handle, std::nullptr_t> {
public:
  using pointer = caf::abstract_actor*;

  constexpr caf_handle(pointer ptr = nullptr) : ptr_(ptr) {
    // nop
  }

  caf_handle(const caf::strong_actor_ptr& x) {
    set(x);
  }

  caf_handle(const caf::actor& x) {
    set(x);
  }

  caf_handle(const caf::actor_addr& x) {
    set(x);
  }

  caf_handle(const caf::scoped_actor& x) {
    set(x);
  }

  template <class... Ts>
  caf_handle(const caf::typed_actor<Ts...>& x) {
    set(x);
  }

  caf_handle(const caf_handle&) = default;

  caf_handle& operator=(pointer x) {
    ptr_ = x;
    return *this;
  }

  template <class T,
            class E = caf::detail::enable_if_t<!std::is_pointer<T>::value>>
  caf_handle& operator=(const T& x) {
    set(x);
    return *this;
  }

  caf_handle& operator=(const caf_handle&) = default;

  pointer get() const {
    return ptr_;
  }

  pointer operator->() const {
    return get();
  }

  ptrdiff_t compare(const caf_handle& other) const {
    return reinterpret_cast<ptrdiff_t>(ptr_)
           - reinterpret_cast<ptrdiff_t>(other.ptr_);
  }

  ptrdiff_t compare(std::nullptr_t) const {
    return reinterpret_cast<ptrdiff_t>(ptr_);
  }

private:
  template <class T>
  void set(const T& x) {
    ptr_ = caf::actor_cast<pointer>(x);
  }

  caf::abstract_actor* ptr_;
};

// -- introspection of the next mailbox element --------------------------------

/// @private
template <class... Ts>
caf::optional<std::tuple<Ts...>> default_extract(caf_handle x) {
  auto ptr = x->peek_at_next_mailbox_element();
  if (ptr == nullptr)
    return caf::none;
  if (auto view = caf::make_const_typed_message_view<Ts...>(ptr->content()))
    return to_tuple(view);
  return caf::none;
}

/// @private
template <class T>
caf::optional<std::tuple<T>> unboxing_extract(caf_handle x) {
  auto tup = default_extract<typename T::outer_type>(x);
  if (tup == caf::none || !is<T>(get<0>(*tup)))
    return caf::none;
  return std::make_tuple(get<T>(get<0>(*tup)));
}

/// Dispatches to `unboxing_extract` if
/// `sizeof...(Ts) == 0 && has_outer_type<T>::value`, otherwise dispatches to
/// `default_extract`.
/// @private
template <class T, bool HasOuterType, class... Ts>
struct try_extract_impl {
  caf::optional<std::tuple<T, Ts...>> operator()(caf_handle x) {
    return default_extract<T, Ts...>(x);
  }
};

template <class T>
struct try_extract_impl<T, true> {
  caf::optional<std::tuple<T>> operator()(caf_handle x) {
    return unboxing_extract<T>(x);
  }
};

/// Returns the content of the next mailbox element as `tuple<T, Ts...>` on a
/// match. Returns `none` otherwise.
template <class T, class... Ts>
caf::optional<std::tuple<T, Ts...>> try_extract(caf_handle x) {
  try_extract_impl<T, has_outer_type<T>::value, Ts...> f;
  return f(x);
}

/// Returns the content of the next mailbox element without taking it out of
/// the mailbox. Fails on an empty mailbox or if the content of the next
/// element does not match `<T, Ts...>`.
template <class T, class... Ts>
std::tuple<T, Ts...> extract(caf_handle x) {
  auto result = try_extract<T, Ts...>(x);
  if (result == caf::none) {
    auto ptr = x->peek_at_next_mailbox_element();
    if (ptr == nullptr)
      CAF_FAIL("Mailbox is empty");
    CAF_FAIL(
      "Message does not match expected pattern: " << to_string(ptr->content()));
  }
  return std::move(*result);
}

template <class T, class... Ts>
bool received(caf_handle x) {
  return try_extract<T, Ts...>(x) != caf::none;
}

template <class... Ts>
class expect_clause {
public:
  explicit expect_clause(caf::scheduler::test_coordinator& sched)
    : sched_(sched), dest_(nullptr) {
    peek_ = [=] {
      /// The extractor will call CAF_FAIL on a type mismatch, essentially
      /// performing a type check when ignoring the result.
      extract<Ts...>(dest_);
    };
  }

  expect_clause(expect_clause&& other) = default;

  ~expect_clause() {
    if (peek_ != nullptr) {
      peek_();
      run_once();
    }
  }

  expect_clause& from(const wildcard&) {
    return *this;
  }

  template <class Handle>
  expect_clause& from(const Handle& whom) {
    src_ = caf::actor_cast<caf::strong_actor_ptr>(whom);
    return *this;
  }

  template <class Handle>
  expect_clause& to(const Handle& whom) {
    CAF_REQUIRE(sched_.prioritize(whom));
    dest_ = &sched_.next_job<caf::abstract_actor>();
    auto ptr = dest_->peek_at_next_mailbox_element();
    CAF_REQUIRE(ptr != nullptr);
    if (src_)
      CAF_REQUIRE_EQUAL(ptr->sender, src_);
    return *this;
  }

  expect_clause& to(const caf::scoped_actor& whom) {
    dest_ = caf::actor_cast<caf::abstract_actor*>(whom);
    return *this;
  }

  template <class... Us>
  void with(Us&&... xs) {
    // TODO: replace this workaround with the make_tuple() line when dropping
    //       support for GCC 4.8.
    std::tuple<typename std::decay<Us>::type...> tmp{std::forward<Us>(xs)...};
    // auto tmp = std::make_tuple(std::forward<Us>(xs)...);
    // TODO: move tmp into lambda when switching to C++14
    peek_ = [=] {
      using namespace caf::detail;
      elementwise_compare_inspector<decltype(tmp)> inspector{tmp};
      auto ys = extract<Ts...>(dest_);
      auto ys_indices = get_indices(ys);
      CAF_REQUIRE(apply_args(inspector, ys_indices, ys));
    };
  }

protected:
  void run_once() {
    auto dptr = dynamic_cast<caf::blocking_actor*>(dest_);
    if (dptr == nullptr)
      sched_.run_once();
    else
      dptr->dequeue(); // Drop message.
  }

  caf::scheduler::test_coordinator& sched_;
  caf::strong_actor_ptr src_;
  caf::abstract_actor* dest_;
  std::function<void()> peek_;
};

template <>
class expect_clause<void> {
public:
  explicit expect_clause(caf::scheduler::test_coordinator& sched)
    : sched_(sched), dest_(nullptr) {
    // nop
  }

  expect_clause(expect_clause&& other) = default;

  ~expect_clause() {
    auto ptr = dest_->peek_at_next_mailbox_element();
    if (ptr == nullptr)
      CAF_FAIL("no message found");
    if (!ptr->content().empty())
      CAF_FAIL("non-empty message found: " << to_string(ptr->content()));
    run_once();
  }

  expect_clause& from(const wildcard&) {
    return *this;
  }

  template <class Handle>
  expect_clause& from(const Handle& whom) {
    src_ = caf::actor_cast<caf::strong_actor_ptr>(whom);
    return *this;
  }

  template <class Handle>
  expect_clause& to(const Handle& whom) {
    CAF_REQUIRE(sched_.prioritize(whom));
    dest_ = &sched_.next_job<caf::abstract_actor>();
    auto ptr = dest_->peek_at_next_mailbox_element();
    CAF_REQUIRE(ptr != nullptr);
    if (src_)
      CAF_REQUIRE_EQUAL(ptr->sender, src_);
    return *this;
  }

  expect_clause& to(const caf::scoped_actor& whom) {
    dest_ = caf::actor_cast<caf::abstract_actor*>(whom);
    return *this;
  }

protected:
  void run_once() {
    auto dptr = dynamic_cast<caf::blocking_actor*>(dest_);
    if (dptr == nullptr)
      sched_.run_once();
    else
      dptr->dequeue(); // Drop message.
  }

  caf::scheduler::test_coordinator& sched_;
  caf::strong_actor_ptr src_;
  caf::abstract_actor* dest_;
};

template <class... Ts>
class inject_clause {
public:
  explicit inject_clause(caf::scheduler::test_coordinator& sched)
    : sched_(sched), dest_(nullptr) {
    // nop
  }

  inject_clause(inject_clause&& other) = default;

  template <class Handle>
  inject_clause& from(const Handle& whom) {
    src_ = caf::actor_cast<caf::strong_actor_ptr>(whom);
    return *this;
  }

  template <class Handle>
  inject_clause& to(const Handle& whom) {
    dest_ = caf::actor_cast<caf::strong_actor_ptr>(whom);
    return *this;
  }

  void with(Ts... xs) {
    if (dest_ == nullptr)
      CAF_FAIL("missing .to() in inject() statement");
    auto msg = caf::make_message(std::move(xs)...);
    if (src_ == nullptr)
      caf::anon_send(caf::actor_cast<caf::actor>(dest_), msg);
    else
      caf::send_as(caf::actor_cast<caf::actor>(src_),
                   caf::actor_cast<caf::actor>(dest_), msg);
    if (!sched_.prioritize(dest_))
      CAF_FAIL("inject: failed to schedule destination actor");
    auto dest_ptr = &sched_.next_job<caf::abstract_actor>();
    auto ptr = dest_ptr->peek_at_next_mailbox_element();
    if (ptr == nullptr)
      CAF_FAIL("inject: failed to get next message from destination actor");
    if (ptr->sender != src_)
      CAF_FAIL("inject: found unexpected sender for the next message");
    if (ptr->payload.cptr() != msg.cptr())
      CAF_FAIL("inject: found unexpected message => " << ptr->payload << " !! "
                                                      << msg);
    msg.reset(); // drop local reference before running the actor
    run_once();
  }

protected:
  void run_once() {
    auto ptr = caf::actor_cast<caf::abstract_actor*>(dest_);
    auto dptr = dynamic_cast<caf::blocking_actor*>(ptr);
    if (dptr == nullptr)
      sched_.run_once();
    else
      dptr->dequeue(); // Drop message.
  }

  caf::scheduler::test_coordinator& sched_;
  caf::strong_actor_ptr src_;
  caf::strong_actor_ptr dest_;
};

template <class... Ts>
class allow_clause {
public:
  explicit allow_clause(caf::scheduler::test_coordinator& sched)
    : sched_(sched), dest_(nullptr) {
    peek_ = [=] {
      if (dest_ != nullptr)
        return try_extract<Ts...>(dest_) != caf::none;
      return false;
    };
  }

  allow_clause(allow_clause&& other) = default;

  ~allow_clause() {
    eval();
  }

  allow_clause& from(const wildcard&) {
    return *this;
  }

  template <class Handle>
  allow_clause& from(const Handle& whom) {
    src_ = caf::actor_cast<caf::strong_actor_ptr>(whom);
    return *this;
  }

  template <class Handle>
  allow_clause& to(const Handle& whom) {
    if (sched_.prioritize(whom))
      dest_ = &sched_.next_job<caf::abstract_actor>();
    return *this;
  }

  template <class... Us>
  void with(Us&&... xs) {
    // TODO: replace this workaround with make_tuple() when dropping support
    //       for GCC 4.8.
    std::tuple<typename std::decay<Us>::type...> tmp{std::forward<Us>(xs)...};
    // TODO: move tmp into lambda when switching to C++14
    peek_ = [=] {
      using namespace caf::detail;
      elementwise_compare_inspector<decltype(tmp)> inspector{tmp};
      auto ys = try_extract<Ts...>(dest_);
      if (ys != caf::none) {
        auto ys_indices = get_indices(*ys);
        return apply_args(inspector, ys_indices, *ys);
      }
      return false;
    };
  }

  bool eval() {
    if (peek_ != nullptr) {
      if (peek_()) {
        run_once();
        return true;
      }
    }
    return false;
  }

protected:
  void run_once() {
    auto dptr = dynamic_cast<caf::blocking_actor*>(dest_);
    if (dptr == nullptr)
      sched_.run_once();
    else
      dptr->dequeue(); // Drop message.
  }

  caf::scheduler::test_coordinator& sched_;
  caf::strong_actor_ptr src_;
  caf::abstract_actor* dest_;
  std::function<bool()> peek_;
};

template <class... Ts>
class disallow_clause {
public:
  disallow_clause() {
    check_ = [=] {
      auto ptr = dest_->peek_at_next_mailbox_element();
      if (ptr == nullptr)
        return;
      if (src_ != nullptr && ptr->sender != src_)
        return;
      auto res = try_extract<Ts...>(dest_);
      if (res != caf::none)
        CAF_FAIL("received disallowed message: " << caf::deep_to_string(*ptr));
    };
  }

  disallow_clause(disallow_clause&& other) = default;

  ~disallow_clause() {
    if (check_ != nullptr)
      check_();
  }

  disallow_clause& from(const wildcard&) {
    return *this;
  }

  disallow_clause& from(caf_handle x) {
    src_ = x;
    return *this;
  }

  disallow_clause& to(caf_handle x) {
    dest_ = x;
    return *this;
  }

  template <class... Us>
  void with(Us&&... xs) {
    // TODO: replace this workaround with make_tuple() when dropping support
    //       for GCC 4.8.
    std::tuple<typename std::decay<Us>::type...> tmp{std::forward<Us>(xs)...};
    // TODO: move tmp into lambda when switching to C++14
    check_ = [=] {
      auto ptr = dest_->peek_at_next_mailbox_element();
      if (ptr == nullptr)
        return;
      if (src_ != nullptr && ptr->sender != src_)
        return;
      auto res = try_extract<Ts...>(dest_);
      if (res != caf::none) {
        using namespace caf::detail;
        elementwise_compare_inspector<decltype(tmp)> inspector{tmp};
        auto& ys = *res;
        auto ys_indices = get_indices(ys);
        if (apply_args(inspector, ys_indices, ys))
          CAF_FAIL("received disallowed message: " << CAF_ARG(*res));
      }
    };
  }

protected:
  caf_handle src_;
  caf_handle dest_;
  std::function<void()> check_;
};

template <class... Ts>
struct test_coordinator_fixture_fetch_helper {
  template <class Self, template <class> class Policy, class Interface>
  std::tuple<Ts...>
  operator()(caf::response_handle<Self, Policy<Interface>>& from) const {
    std::tuple<Ts...> result;
    from.receive([&](Ts&... xs) { result = std::make_tuple(std::move(xs)...); },
                 [&](caf::error& err) { CAF_FAIL(err); });
    return result;
  }
};

template <class T>
struct test_coordinator_fixture_fetch_helper<T> {
  template <class Self, template <class> class Policy, class Interface>
  T operator()(caf::response_handle<Self, Policy<Interface>>& from) const {
    T result;
    from.receive([&](T& x) { result = std::move(x); },
                 [&](caf::error& err) { CAF_FAIL(err); });
    return result;
  }
};

/// A fixture with a deterministic scheduler setup.
template <class Config = caf::actor_system_config>
class test_coordinator_fixture {
public:
  // -- member types -----------------------------------------------------------

  /// A deterministic scheduler type.
  using scheduler_type = caf::scheduler::test_coordinator;

  // -- constructors, destructors, and assignment operators --------------------

  static Config& init_config(Config& cfg) {
    if (auto err = cfg.parse(caf::test::engine::argc(),
                             caf::test::engine::argv()))
      CAF_FAIL("failed to parse config: " << to_string(err));
    cfg.set("caf.scheduler.policy", "testing");
    cfg.set("caf.logger.inline-output", true);
    if (cfg.custom_options().has_category("caf.middleman")) {
      cfg.set("caf.middleman.network-backend", "testing");
      cfg.set("caf.middleman.manual-multiplexing", true);
      cfg.set("caf.middleman.workers", size_t{0});
      cfg.set("caf.middleman.heartbeat-interval", caf::timespan{0});
    }
    cfg.set("caf.stream.credit-policy", "token-based");
    cfg.set("caf.stream.token-based-policy.batch-size", 50);
    cfg.set("caf.stream.token-based-policy.buffer-size", 200);
    return cfg;
  }

  template <class... Ts>
  explicit test_coordinator_fixture(Ts&&... xs)
    : cfg(std::forward<Ts>(xs)...),
      sys(init_config(cfg)),
      self(sys, true),
      sched(dynamic_cast<scheduler_type&>(sys.scheduler())) {
    // Make sure the current time isn't 0.
    sched.clock().current_time += std::chrono::hours(1);
  }

  virtual ~test_coordinator_fixture() {
    run();
  }

  // -- DSL functions ----------------------------------------------------------

  /// Allows the next actor to consume one message from its mailbox. Fails the
  /// test if no message was consumed.
  virtual bool consume_message() {
    return sched.try_run_once();
  }

  /// Allows each actors to consume all messages from its mailbox. Fails the
  /// test if no message was consumed.
  /// @returns The number of consumed messages.
  size_t consume_messages() {
    size_t result = 0;
    while (consume_message())
      ++result;
    return result;
  }

  /// Allows an simulated I/O devices to handle an event.
  virtual bool handle_io_event() {
    return false;
  }

  /// Allows each simulated I/O device to handle all events.
  size_t handle_io_events() {
    size_t result = 0;
    while (handle_io_event())
      ++result;
    return result;
  }

  /// Triggers the next pending timeout.
  virtual bool trigger_timeout() {
    return sched.trigger_timeout();
  }

  /// Triggers all pending timeouts.
  size_t trigger_timeouts() {
    size_t timeouts = 0;
    while (trigger_timeout())
      ++timeouts;
    return timeouts;
  }

  /// Advances the clock by `interval`.
  size_t advance_time(caf::timespan interval) {
    return sched.clock().advance_time(interval);
  }

  /// Consume messages and trigger timeouts until no activity remains.
  /// @returns The total number of events, i.e., messages consumed and
  ///          timeouts triggered.
  size_t run() {
    return run_until([] { return false; });
  }

  /// Consume ones message or triggers the next timeout.
  /// @returns `true` if a message was consumed or timeouts was triggered,
  ///          `false` otherwise.
  bool run_once() {
    return run_until([] { return true; }) > 0;
  }

  /// Consume messages and trigger timeouts until `pred` becomes `true` or
  /// until no activity remains.
  /// @returns The total number of events, i.e., messages consumed and
  ///          timeouts triggered.
  template <class BoolPredicate>
  size_t run_until(BoolPredicate predicate) {
    CAF_LOG_TRACE("");
    // Bookkeeping.
    size_t events = 0;
    // Loop until no activity remains.
    for (;;) {
      size_t progress = 0;
      while (consume_message()) {
        ++progress;
        if (predicate()) {
          CAF_LOG_DEBUG("stop due to predicate:" << CAF_ARG(events));
          return events;
        }
      }
      while (handle_io_event()) {
        ++progress;
        if (predicate()) {
          CAF_LOG_DEBUG("stop due to predicate:" << CAF_ARG(events));
          return events;
        }
      }
      if (trigger_timeout())
        ++progress;
      if (progress == 0) {
        CAF_LOG_DEBUG("no activity left:" << CAF_ARG(events));
        return events;
      }
      events += progress;
    }
  }

  /// Call `run()` when the next scheduled actor becomes ready.
  void run_after_next_ready_event() {
    sched.after_next_enqueue([=] { run(); });
  }

  /// Call `run_until(predicate)` when the next scheduled actor becomes ready.
  template <class BoolPredicate>
  void run_until_after_next_ready_event(BoolPredicate predicate) {
    sched.after_next_enqueue([=] { run_until(predicate); });
  }

  /// Sends a request to `hdl`, then calls `run()`, and finally fetches and
  /// returns the result.
  template <class T, class... Ts, class Handle, class... Us>
  typename std::conditional<sizeof...(Ts) == 0, T, std::tuple<T, Ts...>>::type
  request(Handle hdl, Us... args) {
    auto res_hdl = self->request(hdl, caf::infinite, std::move(args)...);
    run();
    test_coordinator_fixture_fetch_helper<T, Ts...> f;
    return f(res_hdl);
  }

  /// Returns the next message from the next pending actor's mailbox as `T`.
  template <class T>
  const T& peek() {
    return sched.template peek<T>();
  }

  /// Dereferences `hdl` and downcasts it to `T`.
  template <class T = caf::scheduled_actor, class Handle = caf::actor>
  T& deref(const Handle& hdl) {
    auto ptr = caf::actor_cast<caf::abstract_actor*>(hdl);
    CAF_REQUIRE(ptr != nullptr);
    return dynamic_cast<T&>(*ptr);
  }

  template <class... Ts>
  caf::byte_buffer serialize(const Ts&... xs) {
    caf::byte_buffer buf;
    caf::binary_serializer sink{sys, buf};
    if (!(sink.apply(xs) && ...))
      CAF_FAIL("serialization failed: " << sink.get_error());
    return buf;
  }

  template <class... Ts>
  void deserialize(const caf::byte_buffer& buf, Ts&... xs) {
    caf::binary_deserializer source{sys, buf};
    if (!(source.apply(xs) && ...))
      CAF_FAIL("deserialization failed: " << source.get_error());
  }

  template <class T>
  T roundtrip(const T& x) {
    T result;
    deserialize(serialize(x), result);
    return result;
  }

  // -- member variables -------------------------------------------------------

  /// The user-generated system config.
  Config cfg;

  /// Host system for (scheduled) actors.
  caf::actor_system sys;

  /// A scoped actor for conveniently sending and receiving messages.
  caf::scoped_actor self;

  /// Deterministic scheduler.
  scheduler_type& sched;
};

/// Unboxes an expected value or fails the test if it doesn't exist.
template <class T>
T unbox(caf::expected<T> x) {
  if (!x)
    CAF_FAIL(to_string(x.error()));
  return std::move(*x);
}

/// Unboxes an optional value or fails the test if it doesn't exist.
template <class T>
T unbox(caf::optional<T> x) {
  if (!x)
    CAF_FAIL("x == none");
  return std::move(*x);
}

/// Unboxes an optional value or fails the test if it doesn't exist.
template <class T>
T unbox(T* x) {
  if (x == nullptr)
    CAF_FAIL("x == nullptr");
  return *x;
}

/// Expands to its argument.
#define CAF_EXPAND(x) x

/// Expands to its arguments.
#define CAF_DSL_LIST(...) __VA_ARGS__

/// Convenience macro for defining expect clauses.
#define expect(types, fields)                                                  \
  do {                                                                         \
    CAF_MESSAGE("expect" << #types << "." << #fields);                         \
    expect_clause<CAF_EXPAND(CAF_DSL_LIST types)>{sched}.fields;               \
  } while (false)

#define inject(types, fields)                                                  \
  do {                                                                         \
    CAF_MESSAGE("inject" << #types << "." << #fields);                         \
    inject_clause<CAF_EXPAND(CAF_DSL_LIST types)>{sched}.fields;               \
  } while (false)

/// Convenience macro for defining allow clauses.
#define allow(types, fields)                                                   \
  ([&] {                                                                       \
    CAF_MESSAGE("allow" << #types << "." << #fields);                          \
    allow_clause<CAF_EXPAND(CAF_DSL_LIST types)> x{sched};                     \
    x.fields;                                                                  \
    return x.eval();                                                           \
  })()

/// Convenience macro for defining disallow clauses.
#define disallow(types, fields)                                                \
  do {                                                                         \
    CAF_MESSAGE("disallow" << #types << "." << #fields);                       \
    disallow_clause<CAF_EXPAND(CAF_DSL_LIST types)>{}.fields;                  \
  } while (false)

/// Defines the required base type for testee states in the current namespace.
#define TESTEE_SETUP()                                                         \
  template <class T>                                                           \
  struct testee_state_base {}

/// Convenience macro for adding additional state to a testee.
#define TESTEE_STATE(tname)                                                    \
  struct tname##_state;                                                        \
  template <>                                                                  \
  struct testee_state_base<tname##_state>

/// Implementation detail for `TESTEE` and `VARARGS_TESTEE`.
#define TESTEE_SCAFFOLD(tname)                                                 \
  struct tname##_state : testee_state_base<tname##_state> {                    \
    static inline const char* name = #tname;                                   \
  };                                                                           \
  using tname##_actor = stateful_actor<tname##_state>

/// Convenience macro for defining an actor named `tname`.
#define TESTEE(tname)                                                          \
  TESTEE_SCAFFOLD(tname);                                                      \
  behavior tname(tname##_actor* self)

/// Convenience macro for defining an actor named `tname` with any number of
/// initialization arguments.
#define VARARGS_TESTEE(tname, ...)                                             \
  TESTEE_SCAFFOLD(tname);                                                      \
  behavior tname(tname##_actor* self, __VA_ARGS__)

CAF_POP_WARNINGS
