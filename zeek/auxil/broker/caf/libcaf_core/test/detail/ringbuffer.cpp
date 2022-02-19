// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE detail.ringbuffer

#include "caf/detail/ringbuffer.hpp"

#include "caf/test/dsl.hpp"

#include <algorithm>

using namespace caf;

namespace {

static constexpr size_t buf_size = 64;

using int_ringbuffer = detail::ringbuffer<int, buf_size>;

std::vector<int> consumer(int_ringbuffer& buf, size_t num) {
  std::vector<int> result;
  for (size_t i = 0; i < num; ++i) {
    buf.wait_nonempty();
    result.emplace_back(buf.front());
    buf.pop_front();
  }
  return result;
}

void producer(int_ringbuffer& buf, int first, int last) {
  for (auto i = first; i != last; ++i)
    buf.push_back(std::move(i));
}

struct fixture {
  int_ringbuffer buf;
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(ringbuffer_tests, fixture)

CAF_TEST(construction) {
  CAF_CHECK_EQUAL(buf.empty(), true);
  CAF_CHECK_EQUAL(buf.full(), false);
  CAF_CHECK_EQUAL(buf.size(), 0u);
}

CAF_TEST(push_back) {
  CAF_MESSAGE("add one element");
  buf.push_back(42);
  CAF_CHECK_EQUAL(buf.empty(), false);
  CAF_CHECK_EQUAL(buf.full(), false);
  CAF_CHECK_EQUAL(buf.size(), 1u);
  CAF_CHECK_EQUAL(buf.front(), 42);
  CAF_MESSAGE("remove element");
  buf.pop_front();
  CAF_CHECK_EQUAL(buf.empty(), true);
  CAF_CHECK_EQUAL(buf.full(), false);
  CAF_CHECK_EQUAL(buf.size(), 0u);
  CAF_MESSAGE("fill buffer");
  for (int i = 0; i < static_cast<int>(buf_size - 1); ++i)
    buf.push_back(std::move(i));
  CAF_CHECK_EQUAL(buf.empty(), false);
  CAF_CHECK_EQUAL(buf.full(), true);
  CAF_CHECK_EQUAL(buf.size(), buf_size - 1);
  CAF_CHECK_EQUAL(buf.front(), 0);
}

CAF_TEST(get all) {
  using array_type = std::array<int, buf_size>;
  using vector_type = std::vector<int>;
  array_type tmp;
  auto fetch_all = [&] {
    auto i = tmp.begin();
    auto e = buf.get_all(i);
    return vector_type(i, e);
  };
  CAF_MESSAGE("add five element");
  for (int i = 0; i < 5; ++i)
    buf.push_back(std::move(i));
  CAF_CHECK_EQUAL(buf.empty(), false);
  CAF_CHECK_EQUAL(buf.full(), false);
  CAF_CHECK_EQUAL(buf.size(), 5u);
  CAF_CHECK_EQUAL(buf.front(), 0);
  CAF_MESSAGE("drain elements");
  CAF_CHECK_EQUAL(fetch_all(), vector_type({0, 1, 2, 3, 4}));
  CAF_CHECK_EQUAL(buf.empty(), true);
  CAF_CHECK_EQUAL(buf.full(), false);
  CAF_CHECK_EQUAL(buf.size(), 0u);
  CAF_MESSAGE("add 60 elements (wraps around)");
  vector_type expected;
  for (int i = 0; i < 60; ++i) {
    expected.push_back(i);
    buf.push_back(std::move(i));
  }
  CAF_CHECK_EQUAL(buf.size(), 60u);
  CAF_CHECK_EQUAL(fetch_all(), expected);
  CAF_CHECK_EQUAL(buf.empty(), true);
  CAF_CHECK_EQUAL(buf.full(), false);
  CAF_CHECK_EQUAL(buf.size(), 0u);
}

CAF_TEST(concurrent access) {
  std::vector<std::thread> producers;
  producers.emplace_back(producer, std::ref(buf), 0, 100);
  producers.emplace_back(producer, std::ref(buf), 100, 200);
  producers.emplace_back(producer, std::ref(buf), 200, 300);
  auto vec = consumer(buf, 300);
  std::sort(vec.begin(), vec.end());
  CAF_CHECK(std::is_sorted(vec.begin(), vec.end()));
  CAF_CHECK_EQUAL(vec.size(), 300u);
  CAF_CHECK_EQUAL(vec.front(), 0);
  CAF_CHECK_EQUAL(vec.back(), 299);
  for (auto& t : producers)
    t.join();
}

CAF_TEST_FIXTURE_SCOPE_END()
