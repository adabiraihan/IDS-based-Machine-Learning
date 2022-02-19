// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE actor_clock

#include "caf/actor_clock.hpp"

#include "core-test.hpp"

#include <chrono>
#include <memory>

#include "caf/all.hpp"
#include "caf/detail/test_actor_clock.hpp"

using namespace caf;

using namespace std::chrono_literals;

namespace {

struct testee_state {
  uint64_t timeout_id = 41;
};

behavior testee(stateful_actor<testee_state>* self,
                detail::test_actor_clock* t) {
  self->set_exit_handler([self](exit_msg& x) { self->quit(x.reason); });
  self->set_error_handler([](scheduled_actor*, error&) {});
  return {
    [=](ok_atom) {
      CAF_LOG_TRACE("" << self->current_mailbox_element()->content());
      auto n = t->now() + 10s;
      self->state.timeout_id += 1;
      t->set_ordinary_timeout(n, self, "", self->state.timeout_id);
    },
    [=](add_atom) {
      CAF_LOG_TRACE("");
      auto n = t->now() + 10s;
      self->state.timeout_id += 1;
      t->set_multi_timeout(n, self, "", self->state.timeout_id);
    },
    [=](put_atom) {
      CAF_LOG_TRACE("");
      auto n = t->now() + 10s;
      self->state.timeout_id += 1;
      auto mid = make_message_id(self->state.timeout_id).response_id();
      t->set_request_timeout(n, self, mid);
    },
    [](const std::string&) { CAF_LOG_TRACE(""); },
    [=](group& grp) {
      CAF_LOG_TRACE("");
      self->join(grp);
    },
  };
}

struct fixture : test_coordinator_fixture<> {
  detail::test_actor_clock t;
  actor aut;

  fixture() : aut(sys.spawn<lazy_init>(testee, &t)) {
    // nop
  }
};

struct tid {
  uint32_t value;
};

inline bool operator==(const timeout_msg& x, const tid& y) {
  return x.timeout_id == y.value;
}

} // namespace

CAF_TEST_FIXTURE_SCOPE(timer_tests, fixture)

CAF_TEST(single_receive_timeout) {
  // Have AUT call t.set_receive_timeout().
  self->send(aut, ok_atom_v);
  expect((ok_atom), from(self).to(aut).with(_));
  CAF_CHECK_EQUAL(t.schedule().size(), 1u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 1u);
  // Advance time to send timeout message.
  t.advance_time(10s);
  CAF_CHECK_EQUAL(t.schedule().size(), 0u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 0u);
  // Have AUT receive the timeout.
  expect((timeout_msg), from(aut).to(aut).with(tid{42}));
}

CAF_TEST(override_receive_timeout) {
  // Have AUT call t.set_receive_timeout().
  self->send(aut, ok_atom_v);
  expect((ok_atom), from(self).to(aut).with(_));
  CAF_CHECK_EQUAL(t.schedule().size(), 1u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 1u);
  // Have AUT call t.set_timeout() again.
  self->send(aut, ok_atom_v);
  expect((ok_atom), from(self).to(aut).with(_));
  CAF_CHECK_EQUAL(t.schedule().size(), 1u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 1u);
  // Advance time to send timeout message.
  t.advance_time(10s);
  CAF_CHECK_EQUAL(t.schedule().size(), 0u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 0u);
  // Have AUT receive the timeout.
  expect((timeout_msg), from(aut).to(aut).with(tid{43}));
}

CAF_TEST(multi_timeout) {
  // Have AUT call t.set_multi_timeout().
  self->send(aut, add_atom_v);
  expect((add_atom), from(self).to(aut).with(_));
  CAF_CHECK_EQUAL(t.schedule().size(), 1u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 1u);
  // Advance time just a little bit.
  t.advance_time(5s);
  // Have AUT call t.set_multi_timeout() again.
  self->send(aut, add_atom_v);
  expect((add_atom), from(self).to(aut).with(_));
  CAF_CHECK_EQUAL(t.schedule().size(), 2u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 2u);
  // Advance time to send timeout message.
  t.advance_time(5s);
  CAF_CHECK_EQUAL(t.schedule().size(), 1u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 1u);
  // Have AUT receive the timeout.
  expect((timeout_msg), from(aut).to(aut).with(tid{42}));
  // Advance time to send second timeout message.
  t.advance_time(5s);
  CAF_CHECK_EQUAL(t.schedule().size(), 0u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 0u);
  // Have AUT receive the timeout.
  expect((timeout_msg), from(aut).to(aut).with(tid{43}));
}

CAF_TEST(mixed_receive_and_multi_timeouts) {
  // Have AUT call t.set_receive_timeout().
  self->send(aut, add_atom_v);
  expect((add_atom), from(self).to(aut).with(_));
  CAF_CHECK_EQUAL(t.schedule().size(), 1u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 1u);
  // Advance time just a little bit.
  t.advance_time(5s);
  // Have AUT call t.set_multi_timeout() again.
  self->send(aut, ok_atom_v);
  expect((ok_atom), from(self).to(aut).with(_));
  CAF_CHECK_EQUAL(t.schedule().size(), 2u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 2u);
  // Advance time to send timeout message.
  t.advance_time(5s);
  CAF_CHECK_EQUAL(t.schedule().size(), 1u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 1u);
  // Have AUT receive the timeout.
  expect((timeout_msg), from(aut).to(aut).with(tid{42}));
  // Advance time to send second timeout message.
  t.advance_time(5s);
  CAF_CHECK_EQUAL(t.schedule().size(), 0u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 0u);
  // Have AUT receive the timeout.
  expect((timeout_msg), from(aut).to(aut).with(tid{43}));
}

CAF_TEST(single_request_timeout) {
  // Have AUT call t.set_request_timeout().
  self->send(aut, put_atom_v);
  expect((put_atom), from(self).to(aut).with(_));
  CAF_CHECK_EQUAL(t.schedule().size(), 1u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 1u);
  // Advance time to send timeout message.
  t.advance_time(10s);
  CAF_CHECK_EQUAL(t.schedule().size(), 0u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 0u);
  // Have AUT receive the timeout.
  expect((error), from(aut).to(aut).with(sec::request_timeout));
}

CAF_TEST(mixed_receive_and_request_timeouts) {
  // Have AUT call t.set_receive_timeout().
  self->send(aut, ok_atom_v);
  expect((ok_atom), from(self).to(aut).with(_));
  CAF_CHECK_EQUAL(t.schedule().size(), 1u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 1u);
  // Cause the request timeout to arrive later.
  t.advance_time(5s);
  // Have AUT call t.set_request_timeout().
  self->send(aut, put_atom_v);
  expect((put_atom), from(self).to(aut).with(_));
  CAF_CHECK_EQUAL(t.schedule().size(), 2u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 2u);
  // Advance time to send receive timeout message.
  t.advance_time(5s);
  CAF_CHECK_EQUAL(t.schedule().size(), 1u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 1u);
  // Have AUT receive the timeout.
  expect((timeout_msg), from(aut).to(aut).with(tid{42}));
  // Advance time to send request timeout message.
  t.advance_time(10s);
  CAF_CHECK_EQUAL(t.schedule().size(), 0u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 0u);
  // Have AUT receive the timeout.
  expect((error), from(aut).to(aut).with(sec::request_timeout));
}

CAF_TEST(delay_actor_message) {
  // Schedule a message for now + 10s.
  auto n = t.now() + 10s;
  auto autptr = actor_cast<strong_actor_ptr>(aut);
  t.schedule_message(n, autptr,
                     make_mailbox_element(autptr, make_message_id(), no_stages,
                                          "foo"));
  CAF_CHECK_EQUAL(t.schedule().size(), 1u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 0u);
  // Advance time to send the message.
  t.advance_time(10s);
  CAF_CHECK_EQUAL(t.schedule().size(), 0u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 0u);
  // Have AUT receive the message.
  expect((std::string), from(aut).to(aut).with("foo"));
}

CAF_TEST(delay_group_message) {
  // Have AUT join the group.
  auto grp = sys.groups().anonymous();
  self->send(aut, grp);
  expect((group), from(self).to(aut).with(_));
  // Schedule a message for now + 10s.
  auto n = t.now() + 10s;
  auto autptr = actor_cast<strong_actor_ptr>(aut);
  t.schedule_message(n, std::move(grp), autptr, make_message("foo"));
  CAF_CHECK_EQUAL(t.schedule().size(), 1u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 0u);
  // Advance time to send the message.
  t.advance_time(10s);
  CAF_CHECK_EQUAL(t.schedule().size(), 0u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 0u);
  // Have AUT receive the message.
  expect((std::string), from(aut).to(aut).with("foo"));
  // Kill AUT (necessary because the group keeps a reference around).
  self->send_exit(aut, exit_reason::kill);
  expect((exit_msg), from(self).to(aut).with(_));
}

CAF_TEST_FIXTURE_SCOPE_END()
