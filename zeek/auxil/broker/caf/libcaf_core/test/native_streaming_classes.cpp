// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

// This test simulates a complex multiplexing over multiple layers of WDRR
// scheduled queues. The goal is to reduce the complex mailbox management of
// CAF to its bare bones in order to test whether the multiplexing of stream
// traffic and asynchronous messages works as intended.
//
// The setup is a fixed WDRR queue with three nestes queues. The first nested
// queue stores asynchronous messages, the second one upstream messages, and
// the last queue is a dynamic WDRR queue storing downstream messages.
//
// We mock just enough of an actor to use the streaming classes and put them to
// work in a pipeline with 2 or 3 stages.

#define CAF_SUITE native_streaming_classes

#include "core-test.hpp"

#include <memory>
#include <numeric>

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/broadcast_downstream_manager.hpp"
#include "caf/buffered_downstream_manager.hpp"
#include "caf/detail/gcd.hpp"
#include "caf/detail/overload.hpp"
#include "caf/detail/stream_sink_impl.hpp"
#include "caf/detail/stream_source_impl.hpp"
#include "caf/detail/stream_stage_impl.hpp"
#include "caf/downstream_manager.hpp"
#include "caf/downstream_msg.hpp"
#include "caf/inbound_path.hpp"
#include "caf/intrusive/drr_queue.hpp"
#include "caf/intrusive/singly_linked.hpp"
#include "caf/intrusive/task_result.hpp"
#include "caf/intrusive/wdrr_dynamic_multiplexed_queue.hpp"
#include "caf/intrusive/wdrr_fixed_multiplexed_queue.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/mixin/sender.hpp"
#include "caf/no_stages.hpp"
#include "caf/outbound_path.hpp"
#include "caf/policy/arg.hpp"
#include "caf/policy/categorized.hpp"
#include "caf/policy/downstream_messages.hpp"
#include "caf/policy/normal_messages.hpp"
#include "caf/policy/upstream_messages.hpp"
#include "caf/policy/urgent_messages.hpp"
#include "caf/scheduled_actor.hpp"
#include "caf/send.hpp"
#include "caf/stream_manager.hpp"
#include "caf/stream_sink_driver.hpp"
#include "caf/stream_slot.hpp"
#include "caf/stream_source_driver.hpp"
#include "caf/stream_stage_driver.hpp"
#include "caf/system_messages.hpp"
#include "caf/upstream_msg.hpp"
#include "caf/variant.hpp"

using std::vector;

using namespace caf;
using namespace caf::intrusive;

namespace {

// -- utility ------------------------------------------------------------------

struct print_with_comma_t {
  bool first = true;
  template <class T>
  std::ostream&  operator()(std::ostream& out, const T& x) {
    if (!first)
      out << ", ";
    else
      first = false;
    return out << deep_to_string(x);
  }
};

template <class T, class... Ts>
std::string collapse_args(const T& x, const Ts&... xs) {
  std::ostringstream out;
  print_with_comma_t f;
  f(out, x);
  unit(f(out, xs)...);
  return out.str();
}

#define TRACE(name, type, ...)                                                 \
  CAF_MESSAGE(name << " received a " << #type << ": "                          \
                   << collapse_args(__VA_ARGS__));

const char* name_of(const strong_actor_ptr& x) {
  CAF_ASSERT(x != nullptr);
  auto ptr = actor_cast<abstract_actor*>(x);
  return static_cast<local_actor*>(ptr)->name();
}

const char* name_of(const actor_addr& x) {
  return name_of(actor_cast<strong_actor_ptr>(x));
}

// -- queues -------------------------------------------------------------------

using mboxqueue = scheduled_actor::mailbox_policy::queue_type;

template <size_t Value>
using uint_constant =  std::integral_constant<size_t, Value>;

using urgent_async_id = uint_constant<scheduled_actor::urgent_queue_index>;

using normal_async_id = uint_constant<scheduled_actor::normal_queue_index>;

using umsg_id = uint_constant<scheduled_actor::upstream_queue_index>;

using dmsg_id = uint_constant<scheduled_actor::downstream_queue_index>;

// -- entity and mailbox visitor -----------------------------------------------

class entity : public scheduled_actor {
public:
  // -- member types -----------------------------------------------------------

  /// Base type.
  using super = scheduled_actor;

  /// Defines the messaging interface.
  using signatures = none_t;

  /// Defines the container for storing message handlers.
  using behavior_type = behavior;

  /// The type of a single tick.
  using time_point = clock_type::time_point;

  /// A time interval in the resolution of the actor clock.
  using duration_type = time_point::duration;

  /// The type of a single tick.
  using tick_type = size_t;

  // -- constructors, destructors, and assignment operators --------------------

  entity(actor_config& cfg, const char* cstr_name, time_point* global_time)
    : super(cfg),
      mbox(unit, unit, unit, unit, unit),
      name_(cstr_name),
      global_time_(global_time) {
    CAF_ASSERT(global_time_ != nullptr);
  }

  void enqueue(mailbox_element_ptr what, execution_unit*) override {
    auto push_back_result = mbox.push_back(std::move(what));
    CAF_CHECK_EQUAL(push_back_result, true);
    CAF_ASSERT(push_back_result);
  }

  void attach(attachable_ptr) override {
    // nop
  }

  size_t detach(const attachable::token&) override {
    return 0;
  }

  void add_link(abstract_actor*) override {
    // nop
  }

  void remove_link(abstract_actor*) override {
    // nop
  }

  bool add_backlink(abstract_actor*) override {
    return false;
  }

  bool remove_backlink(abstract_actor*) override {
    return false;
  }

  const char* name() const override {
    return name_;
  }

  void launch(execution_unit*, bool, bool) override {
    // nop
  }

  execution_unit* context() {
    return nullptr;
  }

  void start_streaming(entity& ref, int num_messages) {
    CAF_REQUIRE_NOT_EQUAL(num_messages, 0);
    using downstream_manager = broadcast_downstream_manager<int>;
    struct driver final : public stream_source_driver<downstream_manager> {
    public:
      driver(int32_t sentinel) : x_(0), sentinel_(sentinel) {
        // nop
      }

      void pull(downstream<int32_t>& out, size_t hint) override {
        auto y = std::min(sentinel_, x_ + static_cast<int>(hint));
        while (x_ < y)
          out.push(x_++);
      }

      bool done() const noexcept override {
        return x_ == sentinel_;
      }
    private:
      int32_t x_;
      int32_t sentinel_;
    };
    auto mgr = detail::make_stream_source<driver>(this, num_messages);
    auto res = mgr->add_outbound_path(ref.ctrl());
    CAF_MESSAGE(name_ << " starts streaming to " << ref.name()
                << " on slot " << res.value());
  }

  void forward_to(entity& ref) {
    using downstream_manager = broadcast_downstream_manager<int>;
    struct driver final : public stream_stage_driver<int, downstream_manager> {
    public:
      using super = stream_stage_driver<int32_t, downstream_manager>;

      driver(downstream_manager& out, vector<int32_t>* log, const char* name)
        : super(out), log_(log), name(name) {
        // nop
      }

      void process(downstream<int>& out, vector<int>& batch) override {
        CAF_MESSAGE(name << " forwards " << batch.size() << " elements");
        log_->insert(log_->end(), batch.begin(), batch.end());
        out.append(batch.begin(), batch.end());
      }

      void finalize(const error&) override {
        // nop
      }

    private:
      vector<int>* log_;
      const char* name;
    };
    forwarder = detail::make_stream_stage<driver>(this, &data, name_);
    auto res = forwarder->add_outbound_path(ref.ctrl());
    CAF_MESSAGE(name_ << " starts forwarding to " << ref.name()
                << " on slot " << res.value());
  }

  void operator()(open_stream_msg& hs) {
    TRACE(name_, stream_handshake_msg,
          CAF_ARG2("sender", name_of(hs.prev_stage)));
    // Create required state if no forwarder exists yet, otherwise `forward_to`
    // was called and we run as a stage.
    stream_sink_ptr<int> mgr = forwarder;
    if (mgr == nullptr) {
      struct driver final : public stream_sink_driver<int> {
      public:
        driver(std::vector<int>* log) : log_(log) {
          // nop
        }

        void process(std::vector<int>& xs) override {
          log_->insert(log_->end(), xs.begin(), xs.end());
        }
      private:
        vector<int>* log_;
      };
      mgr = detail::make_stream_sink<driver>(this, &data);
    }
    CAF_REQUIRE(hs.msg.match_elements<stream<int>>());
    auto& in = hs.msg.get_as<stream<int>>(0);
    mgr->add_inbound_path(in);
  }

  void operator()(stream_slots slots, actor_addr& sender,
                  upstream_msg::ack_open& x) {
    TRACE(name_, ack_open, CAF_ARG(slots),
          CAF_ARG2("sender", name_of(x.rebind_to)), CAF_ARG(x));
    CAF_REQUIRE_EQUAL(sender, x.rebind_to);
    scheduled_actor::handle_upstream_msg(slots, sender, x);
  }

  void operator()(stream_slots slots, actor_addr& sender,
                  upstream_msg::ack_batch& x) {
    TRACE(name_, ack_batch, CAF_ARG(slots),
          CAF_ARG2("sender", name_of(sender)), CAF_ARG(x));
    scheduled_actor::handle_upstream_msg(slots, sender, x);
  }

  void tick() {
    for (auto& kvp : stream_managers())
      kvp.second->tick(now());
  }

  virtual bool add_inbound_path(type_id_t,
                                std::unique_ptr<inbound_path> path) override {
    using policy_type = policy::downstream_messages::nested;
    auto res = get<dmsg_id::value>(mbox.queues())
                 .queues()
                 .emplace(path->slots.receiver, policy_type{nullptr});
    if (!res.second)
      return false;
    res.first->second.policy().handler = std::move(path);
    return true;
  }

  void erase_inbound_path_later(stream_slot slot) override {
    get<dmsg_id::value>(mbox.queues()).erase_later(slot);
  }

  void erase_inbound_paths_later(const stream_manager* mgr) override {
    for (auto& kvp : get<dmsg_id::value>(mbox.queues()).queues()) {
      auto& path = kvp.second.policy().handler;
      if (path != nullptr && path->mgr == mgr)
        erase_inbound_path_later(kvp.first);
    }
  }

  void erase_inbound_paths_later(const stream_manager* mgr,
                                 error err) override {
    CAF_REQUIRE_EQUAL(err, none);
    erase_inbound_paths_later(mgr);
  }

  time_point now() {
    return *global_time_;
  }

  void push() {
    if (forwarder)
      forwarder->push();
    for (auto mgr : active_stream_managers())
      mgr->push();
  }

  // -- member variables -------------------------------------------------------

  mboxqueue mbox;
  const char* name_;
  vector<int> data; // Keeps track of all received data from all batches.
  stream_stage_ptr<int, broadcast_downstream_manager<int>> forwarder;

  tick_type ticks_per_force_batches_interval;
  tick_type ticks_per_credit_interval;
  time_point* global_time_;
};

struct msg_visitor {
  // -- member types -----------------------------------------------------------

  using result_type = intrusive::task_result;

  // -- operator() overloads ---------------------------------------------------

  result_type operator()(urgent_async_id, entity::urgent_queue&,
                         mailbox_element&) {
    CAF_FAIL("unexpected function call");
    return intrusive::task_result::stop;
  }

  result_type operator()(normal_async_id, entity::normal_queue&,
                         mailbox_element& x) {
    CAF_REQUIRE(x.content().match_elements<open_stream_msg>());
    self->current_mailbox_element(&x);
    (*self)(x.content().get_mutable_as<open_stream_msg>(0));
    self->current_mailbox_element(nullptr);
    return intrusive::task_result::resume;
  }

  result_type operator()(umsg_id, entity::upstream_queue&, mailbox_element& x) {
    CAF_REQUIRE(x.content().match_elements<upstream_msg>());
    self->current_mailbox_element(&x);
    auto& um = x.content().get_mutable_as<upstream_msg>(0);
    auto f = detail::make_overload(
      [&](upstream_msg::ack_open& y) {
        (*self)(um.slots, um.sender, y);
      },
      [&](upstream_msg::ack_batch& y) {
        (*self)(um.slots, um.sender, y);
      },
      [](upstream_msg::drop&) {
        CAF_FAIL("did not expect upstream_msg::drop");
      },
      [](upstream_msg::forced_drop&) {
        CAF_FAIL("did not expect upstream_msg::forced_drop");
      }
    );
    visit(f, um.content);
    self->current_mailbox_element(nullptr);
    self->push();
    return intrusive::task_result::resume;
  }

  result_type operator()(dmsg_id, entity::downstream_queue& qs, stream_slot,
                         policy::downstream_messages::nested_queue_type& q,
                         mailbox_element& x) {
    CAF_REQUIRE(x.content().match_elements<downstream_msg>());
    self->current_mailbox_element(&x);
    auto inptr = q.policy().handler.get();
    if (inptr == nullptr)
      return intrusive::task_result::stop;
    auto& dm = x.content().get_mutable_as<downstream_msg>(0);
    auto f = detail::make_overload(
      [&](downstream_msg::batch& y) {
        TRACE(self->name(), batch, CAF_ARG(dm.slots), CAF_ARG(y.xs_size));
        inptr->handle(y);
        if (inptr->mgr->done()) {
          CAF_MESSAGE(self->name()
                      << " is done receiving and closes its manager");
          inptr->mgr->stop();
        }
        return intrusive::task_result::resume;
      },
      [&](downstream_msg::close& y) {
        TRACE(self->name(), close, CAF_ARG(dm.slots));
        auto slots = dm.slots;
        auto i = self->stream_managers().find(slots.receiver);
        CAF_REQUIRE_NOT_EQUAL(i, self->stream_managers().end());
        i->second->handle(inptr, y);
        q.policy().handler.reset();
        qs.erase_later(slots.receiver);
        if (!i->second->done()) {
          self->stream_managers().erase(i);
        } else {
          // Close the manager and remove it on all registered slots.
          auto mgr = i->second;
          self->erase_stream_manager(mgr);
          mgr->stop();
        }
        return intrusive::task_result::resume;
      },
      [](downstream_msg::forced_close&) {
        CAF_FAIL("did not expect downstream_msg::forced_close");
        return intrusive::task_result::stop;
      });
    auto result = visit(f, dm.content);
    self->current_mailbox_element(nullptr);
    return result;
  }

  // -- member variables -------------------------------------------------------

  entity* self;
};

// -- fixture ------------------------------------------------------------------

struct fixture {
  using scheduler_type = scheduler::test_coordinator;

  timespan max_batch_delay = defaults::stream::max_batch_delay;

  actor_system_config cfg;
  actor_system sys;
  scheduler_type& sched;
  actor alice_hdl;
  actor bob_hdl;
  actor carl_hdl;

  entity& alice;
  entity& bob;
  entity& carl;

  static actor spawn(actor_system& sys, actor_id id, const char* name) {
    actor_config conf;
    auto& clock = dynamic_cast<scheduler_type&>(sys.scheduler()).clock();
    auto global_time = &clock.current_time;
    return make_actor<entity>(id, node_id{}, &sys, conf, name, global_time);
  }

  static entity& fetch(const actor& hdl) {
    return *static_cast<entity*>(actor_cast<abstract_actor*>(hdl));
  }

  static actor_system_config& init_config(actor_system_config& cfg) {
    if (auto err = cfg.parse(caf::test::engine::argc(),
                             caf::test::engine::argv()))
      CAF_FAIL("parsing the config failed: " << to_string(err));
    cfg.set("caf.scheduler.policy", "testing");
    cfg.set("caf.stream.credit-policy", "token-based");
    cfg.set("caf.stream.token-based-policy.batch-size", 50);
    cfg.set("caf.stream.token-based-policy.buffer-size", 200);
    return cfg;
  }

  fixture()
    : sys(init_config(cfg)),
      sched(dynamic_cast<scheduler_type&>(sys.scheduler())),
      alice_hdl(spawn(sys, 0, "alice")),
      bob_hdl(spawn(sys, 1, "bob")),
      carl_hdl(spawn(sys, 2, "carl")),
      alice(fetch(alice_hdl)),
      bob(fetch(bob_hdl)),
      carl(fetch(carl_hdl)) {
    // nop
  }

  ~fixture() {
    // Check whether all actors cleaned up their state properly.
    entity* xs[] = {&alice, &bob, &carl};
    for (auto x : xs) {
      CAF_CHECK(get<dmsg_id::value>(x->mbox.queues()).queues().empty());
      CAF_CHECK(x->pending_stream_managers().empty());
      CAF_CHECK(x->stream_managers().empty());
    }
  }

  template <class... Ts>
  void loop(Ts&... xs) {
    msg_visitor fs[] = {{&xs}...};
    auto mailbox_empty = [](msg_visitor& x) { return x.self->mbox.empty(); };
    while (!std::all_of(std::begin(fs), std::end(fs), mailbox_empty))
      for (auto& f : fs)
        f.self->mbox.new_round(1, f);
  }

  template <class... Ts>
  void next_cycle(Ts&... xs) {
    entity* es[] = {&xs...};
    CAF_MESSAGE("advance clock by " << max_batch_delay);
    sched.clock().current_time += max_batch_delay;
    for (auto e : es)
      e->tick();
  }

  template <class F, class... Ts>
  void loop_until(F pred, Ts&... xs) {
    entity* es[] = {&xs...};
    msg_visitor fs[] = {{&xs}...};
    auto mailbox_empty = [](msg_visitor& x) { return x.self->mbox.empty(); };
    do {
      while (!std::all_of(std::begin(fs), std::end(fs), mailbox_empty))
        for (auto& f : fs)
          f.self->mbox.new_round(1, f);
      CAF_MESSAGE("advance clock by " << max_batch_delay);
      sched.clock().current_time += max_batch_delay;
      for (auto e : es)
        e->tick();
    }
    while (!pred());
  }

  bool done_streaming() {
    entity* es[] = {&alice, &bob, &carl};
    return std::all_of(std::begin(es), std::end(es),
                       [](entity* e) { return e->stream_managers().empty(); });
  }
};

vector<int> make_iota(int first, int last) {
  CAF_ASSERT(first < last);
  vector<int> result;
  result.resize(static_cast<size_t>(last - first));
  std::iota(result.begin(), result.end(), first);
  return result;
}

} // namespace

// -- unit tests ---------------------------------------------------------------

CAF_TEST_FIXTURE_SCOPE(native_streaming_classes_tests, fixture)

CAF_TEST(depth_2_pipeline_30_items) {
  alice.start_streaming(bob, 30);
  loop_until([&] { return done_streaming(); }, alice, bob);
  CAF_CHECK_EQUAL(bob.data, make_iota(0, 30));
}

CAF_TEST(depth_2_pipeline_500_items) {
  constexpr size_t num_messages = 500;
  alice.start_streaming(bob, num_messages);
  loop_until([&] { return done_streaming(); }, alice, bob);
  CAF_CHECK_EQUAL(bob.data, make_iota(0, num_messages));
}

CAF_TEST(depth_3_pipeline_30_items) {
  bob.forward_to(carl);
  alice.start_streaming(bob, 30);
  loop_until([&] { return done_streaming(); }, alice, bob, carl);
  CAF_CHECK_EQUAL(bob.data, make_iota(0, 30));
  CAF_CHECK_EQUAL(carl.data, make_iota(0, 30));
}

CAF_TEST(depth_3_pipeline_500_items) {
  constexpr size_t num_messages = 500;
  bob.forward_to(carl);
  alice.start_streaming(bob, num_messages);
  CAF_MESSAGE("loop over alice and bob until bob is congested");
  loop(alice, bob);
  CAF_CHECK_NOT_EQUAL(bob.data.size(), 0u);
  CAF_CHECK_EQUAL(carl.data.size(), 0u);
  CAF_MESSAGE("loop over bob and carl until bob finished sending");
  // bob has one batch from alice in its mailbox that bob will read when
  // becoming uncongested again
  loop(bob, carl);
  CAF_CHECK_EQUAL(bob.data.size(), carl.data.size());
  CAF_MESSAGE("loop over all until done");
  loop_until([&] { return done_streaming(); }, alice, bob, carl);
  CAF_CHECK_EQUAL(bob.data, make_iota(0, num_messages));
  CAF_CHECK_EQUAL(carl.data, make_iota(0, num_messages));
}

CAF_TEST_FIXTURE_SCOPE_END()
