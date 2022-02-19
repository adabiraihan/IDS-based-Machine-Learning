// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE const_typed_message_view

#include "caf/const_typed_message_view.hpp"

#include "core-test.hpp"

#include "caf/message.hpp"

using namespace caf;

CAF_TEST_FIXTURE_SCOPE(message_tests, test_coordinator_fixture<>)

CAF_TEST(const message views never detach their content) {
  auto msg1 = make_message(1, 2, 3, "four");
  auto msg2 = msg1;
  CAF_REQUIRE(msg1.cptr() == msg2.cptr());
  CAF_REQUIRE(msg1.match_elements<int, int, int, std::string>());
  const_typed_message_view<int, int, int, std::string> view{msg1};
  CAF_REQUIRE(msg1.cptr() == msg2.cptr());
}

CAF_TEST(const message views allow access via get) {
  auto msg = make_message(1, 2, 3, "four");
  CAF_REQUIRE(msg.match_elements<int, int, int, std::string>());
  const_typed_message_view<int, int, int, std::string> view{msg};
  CAF_CHECK_EQUAL(get<0>(view), 1);
  CAF_CHECK_EQUAL(get<1>(view), 2);
  CAF_CHECK_EQUAL(get<2>(view), 3);
  CAF_CHECK_EQUAL(get<3>(view), "four");
}

CAF_TEST_FIXTURE_SCOPE_END()
