// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE policy.select_all

#include "caf/policy/select_all.hpp"

#include "caf/test/dsl.hpp"

#include <tuple>

#include "caf/actor_system.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/sec.hpp"

using caf::policy::select_all;

using namespace caf;

namespace {

struct fixture : test_coordinator_fixture<> {
  template <class F>
  actor make_server(F f) {
    auto init = [f]() -> behavior {
      return {
        [f](int x, int y) { return f(x, y); },
      };
    };
    return sys.spawn(init);
  }

  std::function<void(const error&)> make_error_handler() {
    return [](const error& err) { CAF_FAIL("unexpected error: " << err); };
  }

  std::function<void(const error&)> make_counting_error_handler(size_t* count) {
    return [count](const error&) { *count += 1; };
  }
};

} // namespace

#define SUBTEST(message)                                                       \
  run();                                                                       \
  CAF_MESSAGE("subtest: " message);                                            \
  for (int subtest_dummy = 0; subtest_dummy < 1; ++subtest_dummy)

CAF_TEST_FIXTURE_SCOPE(select_all_tests, fixture)

CAF_TEST(select_all combines two integer results into one vector) {
  using int_list = std::vector<int>;
  auto f = [](int x, int y) { return x + y; };
  auto server1 = make_server(f);
  auto server2 = make_server(f);
  SUBTEST("request.receive") {
    SUBTEST("vector of int") {
      auto r1 = self->request(server1, infinite, 1, 2);
      auto r2 = self->request(server2, infinite, 2, 3);
      select_all<detail::type_list<int>> merge{{r1.id(), r2.id()}};
      run();
      merge.receive(
        self.ptr(),
        [](int_list results) {
          std::sort(results.begin(), results.end());
          CAF_CHECK_EQUAL(results, int_list({3, 5}));
        },
        make_error_handler());
    }
    SUBTEST("vector of tuples") {
      using std::make_tuple;
      auto r1 = self->request(server1, infinite, 1, 2);
      auto r2 = self->request(server2, infinite, 2, 3);
      select_all<detail::type_list<int>> merge{{r1.id(), r2.id()}};
      run();
      using results_vector = std::vector<std::tuple<int>>;
      merge.receive(
        self.ptr(),
        [](results_vector results) {
          std::sort(results.begin(), results.end());
          CAF_CHECK_EQUAL(results,
                          results_vector({make_tuple(3), make_tuple(5)}));
        },
        make_error_handler());
    }
  }
  SUBTEST("request.then") {
    int_list results;
    auto client = sys.spawn([=, &results](event_based_actor* client_ptr) {
      auto r1 = client_ptr->request(server1, infinite, 1, 2);
      auto r2 = client_ptr->request(server2, infinite, 2, 3);
      select_all<detail::type_list<int>> merge{{r1.id(), r2.id()}};
      merge.then(
        client_ptr, [&results](int_list xs) { results = std::move(xs); },
        make_error_handler());
    });
    run_once();
    expect((int, int), from(client).to(server1).with(1, 2));
    expect((int, int), from(client).to(server2).with(2, 3));
    expect((int), from(server1).to(client).with(3));
    expect((int), from(server2).to(client).with(5));
    CAF_MESSAGE("request.then stores results in arrival order");
    CAF_CHECK_EQUAL(results, int_list({3, 5}));
  }
  SUBTEST("request.await") {
    int_list results;
    auto client = sys.spawn([=, &results](event_based_actor* client_ptr) {
      auto r1 = client_ptr->request(server1, infinite, 1, 2);
      auto r2 = client_ptr->request(server2, infinite, 2, 3);
      select_all<detail::type_list<int>> merge{{r1.id(), r2.id()}};
      merge.await(
        client_ptr, [&results](int_list xs) { results = std::move(xs); },
        make_error_handler());
    });
    run_once();
    expect((int, int), from(client).to(server1).with(1, 2));
    expect((int, int), from(client).to(server2).with(2, 3));
    // TODO: DSL (mailbox.peek) cannot deal with receivers that skip messages.
    // expect((int), from(server1).to(client).with(3));
    // expect((int), from(server2).to(client).with(5));
    run();
    CAF_MESSAGE("request.await froces responses into reverse request order");
    CAF_CHECK_EQUAL(results, int_list({5, 3}));
  }
}

CAF_TEST(select_all calls the error handler at most once) {
  using int_list = std::vector<int>;
  auto f = [](int, int) -> result<int> { return sec::invalid_argument; };
  auto server1 = make_server(f);
  auto server2 = make_server(f);
  SUBTEST("request.receive") {
    auto r1 = self->request(server1, infinite, 1, 2);
    auto r2 = self->request(server2, infinite, 2, 3);
    select_all<detail::type_list<int>> merge{{r1.id(), r2.id()}};
    run();
    size_t errors = 0;
    merge.receive(
      self.ptr(),
      [](int_list) { CAF_FAIL("fan-in policy called the result handler"); },
      make_counting_error_handler(&errors));
    CAF_CHECK_EQUAL(errors, 1u);
  }
  SUBTEST("request.then") {
    size_t errors = 0;
    auto client = sys.spawn([=, &errors](event_based_actor* client_ptr) {
      auto r1 = client_ptr->request(server1, infinite, 1, 2);
      auto r2 = client_ptr->request(server2, infinite, 2, 3);
      select_all<detail::type_list<int>> merge{{r1.id(), r2.id()}};
      merge.then(
        client_ptr,
        [](int_list) { CAF_FAIL("fan-in policy called the result handler"); },
        make_counting_error_handler(&errors));
    });
    run_once();
    expect((int, int), from(client).to(server1).with(1, 2));
    expect((int, int), from(client).to(server2).with(2, 3));
    expect((error), from(server1).to(client).with(sec::invalid_argument));
    expect((error), from(server2).to(client).with(sec::invalid_argument));
    CAF_CHECK_EQUAL(errors, 1u);
  }
  SUBTEST("request.await") {
    size_t errors = 0;
    auto client = sys.spawn([=, &errors](event_based_actor* client_ptr) {
      auto r1 = client_ptr->request(server1, infinite, 1, 2);
      auto r2 = client_ptr->request(server2, infinite, 2, 3);
      select_all<detail::type_list<int>> merge{{r1.id(), r2.id()}};
      merge.await(
        client_ptr,
        [](int_list) { CAF_FAIL("fan-in policy called the result handler"); },
        make_counting_error_handler(&errors));
    });
    run_once();
    expect((int, int), from(client).to(server1).with(1, 2));
    expect((int, int), from(client).to(server2).with(2, 3));
    // TODO: DSL (mailbox.peek) cannot deal with receivers that skip messages.
    // expect((int), from(server1).to(client).with(3));
    // expect((int), from(server2).to(client).with(5));
    run();
    CAF_CHECK_EQUAL(errors, 1u);
  }
}

CAF_TEST_FIXTURE_SCOPE_END()
