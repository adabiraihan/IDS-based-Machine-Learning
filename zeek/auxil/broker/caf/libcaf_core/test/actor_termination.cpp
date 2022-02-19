// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE actor_termination

#include "core-test.hpp"

#include "caf/all.hpp"

using namespace caf;

namespace {

behavior mirror_impl(event_based_actor* self) {
  self->set_default_handler(reflect);
  return [] {
    // nop
  };
}

struct fixture :  test_coordinator_fixture<> {
  actor mirror;
  actor testee;

  fixture() {
    mirror = sys.spawn(mirror_impl);
    // run initialization code or mirror
    sched.run_once();
  }

  template <class... Ts>
  void spawn(Ts&&... xs) {
    testee = self->spawn(std::forward<Ts>(xs)...);
  }

  ~fixture() {
    self->wait_for(testee);
  }
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(actor_termination_tests, fixture)

CAF_TEST(single_multiplexed_request) {
  auto f = [&](event_based_actor* self, actor server) {
    self->request(server, infinite, 42).then([=](int x) {
      CAF_LOG_TRACE(CAF_ARG(x));
      CAF_REQUIRE_EQUAL(x, 42);
    });
  };
  spawn(f, mirror);
  // run initialization code of testee
  sched.run_once();
  expect((int), from(testee).to(mirror).with(42));
  expect((int), from(mirror).to(testee).with(42));
}

CAF_TEST(multiple_multiplexed_requests) {
  auto f = [&](event_based_actor* self, actor server) {
    for (int i = 0; i < 3; ++i)
      self->request(server, infinite, 42).then([=](int x) {
        CAF_LOG_TRACE(CAF_ARG(x));
        CAF_REQUIRE_EQUAL(x, 42);
      });
  };
  spawn(f, mirror);
  // run initialization code of testee
  sched.run_once();
  expect((int), from(testee).to(mirror).with(42));
  expect((int), from(testee).to(mirror).with(42));
  expect((int), from(testee).to(mirror).with(42));
  expect((int), from(mirror).to(testee).with(42));
  expect((int), from(mirror).to(testee).with(42));
  expect((int), from(mirror).to(testee).with(42));
}

CAF_TEST(single_awaited_request) {
  auto f = [&](event_based_actor* self, actor server) {
    self->request(server, infinite, 42).await(
      [=](int x) {
        CAF_REQUIRE_EQUAL(x, 42);
      }
    );
  };
  spawn(f, mirror);
  // run initialization code of testee
  sched.run_once();
  expect((int), from(testee).to(mirror).with(42));
  expect((int), from(mirror).to(testee).with(42));
}

CAF_TEST(multiple_awaited_requests) {
  auto f = [&](event_based_actor* self, actor server) {
    for (int i = 0; i < 3; ++i)
      self->request(server, infinite, i).await(
        [=](int x) {
          CAF_MESSAGE("received response #" << (i + 1));
          CAF_REQUIRE_EQUAL(x, i);
        }
      );
  };
  spawn(f, mirror);
  // run initialization code of testee
  sched.run_once();
  self->monitor(testee);
  expect((int), from(testee).to(mirror).with(0));
  expect((int), from(testee).to(mirror).with(1));
  expect((int), from(testee).to(mirror).with(2));
  // request().await() processes messages out-of-order,
  // which means we cannot check using expect()
  sched.run();
  expect((down_msg), from(testee).to(self).with(_));
}

CAF_TEST_FIXTURE_SCOPE_END()
