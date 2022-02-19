// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE mixin.sender

#include "caf/mixin/sender.hpp"

#include "caf/test/dsl.hpp"

#include <chrono>

using namespace caf;

using std::chrono::seconds;

namespace {

behavior testee_impl(event_based_actor* self) {
  self->set_default_handler(reflect);
  return {
    [] {
      // nop
    },
  };
}

struct fixture : test_coordinator_fixture<> {
  group grp;
  actor testee;

  std::string hello = "hello world";

  fixture() {
    grp = sys.groups().anonymous();
    testee = sys.spawn_in_group(grp, testee_impl);
  }

  ~fixture() {
    anon_send_exit(testee, exit_reason::user_shutdown);
  }
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(sender_tests, fixture)

CAF_TEST(delayed actor messages receive responses) {
  self->delayed_send(testee, seconds(1), hello);
  sched.trigger_timeout();
  expect((std::string), from(self).to(testee).with(hello));
  expect((std::string), from(testee).to(self).with(hello));
  self->scheduled_send(testee, self->clock().now() + seconds(1), hello);
  sched.trigger_timeout();
  expect((std::string), from(self).to(testee).with(hello));
  expect((std::string), from(testee).to(self).with(hello));
}

CAF_TEST(delayed group message receive responses) {
  self->delayed_send(grp, seconds(1), hello);
  sched.trigger_timeout();
  expect((std::string), from(self).to(testee).with(hello));
  expect((std::string), from(testee).to(self).with(hello));
  self->scheduled_send(grp, self->clock().now() + seconds(1), hello);
  sched.trigger_timeout();
  expect((std::string), from(self).to(testee).with(hello));
  expect((std::string), from(testee).to(self).with(hello));
}

CAF_TEST(anonymous messages receive no response) {
  self->anon_send(testee, hello);
  expect((std::string), to(testee).with(hello));
  disallow((std::string), from(testee).to(self).with(hello));
  self->delayed_anon_send(testee, seconds(1), hello);
  sched.trigger_timeout();
  expect((std::string), to(testee).with(hello));
  disallow((std::string), from(testee).to(self).with(hello));
  self->scheduled_anon_send(testee, self->clock().now() + seconds(1), hello);
  sched.trigger_timeout();
  expect((std::string), to(testee).with(hello));
  disallow((std::string), from(testee).to(self).with(hello));
  self->delayed_anon_send(grp, seconds(1), hello);
  sched.trigger_timeout();
  expect((std::string), to(testee).with(hello));
  disallow((std::string), from(testee).to(self).with(hello));
  self->scheduled_anon_send(grp, self->clock().now() + seconds(1), hello);
  sched.trigger_timeout();
  expect((std::string), to(testee).with(hello));
  disallow((std::string), from(testee).to(self).with(hello));
}

CAF_TEST_FIXTURE_SCOPE_END()
