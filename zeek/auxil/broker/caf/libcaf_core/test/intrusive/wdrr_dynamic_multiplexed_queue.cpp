// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE intrusive.wdrr_dynamic_multiplexed_queue

#include "caf/intrusive/wdrr_dynamic_multiplexed_queue.hpp"

#include "caf/test/unit_test.hpp"

#include <memory>

#include "caf/intrusive/drr_queue.hpp"
#include "caf/intrusive/singly_linked.hpp"

using namespace caf;
using namespace caf::intrusive;

namespace {

struct inode : singly_linked<inode> {
  int value;
  inode(int x = 0) noexcept : value(x) {
    // nop
  }
};

std::string to_string(const inode& x) {
  return std::to_string(x.value);
}

struct nested_inode_policy {
  using mapped_type = inode;

  using task_size_type = int;

  using deficit_type = int;

  using deleter_type = std::default_delete<mapped_type>;

  using unique_pointer = std::unique_ptr<mapped_type, deleter_type>;

  static task_size_type task_size(const mapped_type&) noexcept {
    return 1;
  }

  std::unique_ptr<int> queue_id;

  nested_inode_policy(int i) : queue_id(new int(i)) {
    // nop
  }

  nested_inode_policy(nested_inode_policy&&) noexcept = default;
  nested_inode_policy& operator=(nested_inode_policy&&) noexcept = default;
};

struct inode_policy {
  using mapped_type = inode;

  using key_type = int;

  using task_size_type = int;

  using deficit_type = int;

  using deleter_type = std::default_delete<mapped_type>;

  using unique_pointer = std::unique_ptr<mapped_type, deleter_type>;

  using queue_type = drr_queue<nested_inode_policy>;

  using queue_map_type = std::map<key_type, queue_type>;

  static key_type id_of(const inode& x) noexcept {
    return x.value % 3;
  }

  static bool enabled(const queue_type&) noexcept {
    return true;
  }

  deficit_type quantum(const queue_type& q, deficit_type x) noexcept {
    return enable_priorities && *q.policy().queue_id == 0 ? 2 * x : x;
  }

  static void cleanup(queue_type&) noexcept {
    // nop
  }

  static bool push_back(queue_type& q, mapped_type* ptr) noexcept {
    return q.push_back(ptr);
  }

  bool enable_priorities = false;
};

using queue_type = wdrr_dynamic_multiplexed_queue<inode_policy>;

using nested_queue_type = inode_policy::queue_type;

struct fixture {
  inode_policy policy;
  queue_type queue{policy};

  int fill(queue_type&) {
    return 0;
  }

  template <class T, class... Ts>
  int fill(queue_type& q, T x, Ts... xs) {
    return (q.emplace_back(x) ? 1 : 0) + fill(q, xs...);
  }

  std::string fetch(int quantum) {
    std::string result;
    auto f = [&](int id, nested_queue_type& q, inode& x) {
      CAF_CHECK_EQUAL(id, *q.policy().queue_id);
      if (!result.empty())
        result += ',';
      result += to_string(id);
      result += ':';
      result += to_string(x);
      return task_result::resume;
    };
    queue.new_round(quantum, f);
    return result;
  }

  void make_queues() {
    for (int i = 0; i < 3; ++i)
      queue.queues().emplace(i, nested_inode_policy{i});
  }
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(wdrr_dynamic_multiplexed_queue_tests, fixture)

CAF_TEST(default_constructed) {
  CAF_REQUIRE_EQUAL(queue.empty(), true);
}

CAF_TEST(dropping) {
  CAF_REQUIRE_EQUAL(queue.empty(), true);
  CAF_REQUIRE_EQUAL(fill(queue, 1, 2, 3, 4, 5, 6, 7, 8, 9, 12), 0);
  CAF_REQUIRE_EQUAL(queue.empty(), true);
}

CAF_TEST(new_round) {
  make_queues();
  fill(queue, 1, 2, 3, 4, 5, 6, 7, 8, 9, 12);
  CAF_REQUIRE_EQUAL(queue.empty(), false);
  CAF_CHECK_EQUAL(fetch(1), "0:3,1:1,2:2");
  CAF_REQUIRE_EQUAL(queue.empty(), false);
  CAF_CHECK_EQUAL(fetch(9), "0:6,0:9,0:12,1:4,1:7,2:5,2:8");
  CAF_REQUIRE_EQUAL(queue.empty(), true);
}

CAF_TEST(priorities) {
  make_queues();
  queue.policy().enable_priorities = true;
  fill(queue, 1, 2, 3, 4, 5, 6, 7, 8, 9);
  // Allow f to consume 2 items from the high priority and 1 item otherwise.
  CAF_CHECK_EQUAL(fetch(1), "0:3,0:6,1:1,2:2");
  CAF_REQUIRE_EQUAL(queue.empty(), false);
  // Drain the high-priority queue with one item left per other queue.
  CAF_CHECK_EQUAL(fetch(1), "0:9,1:4,2:5");
  CAF_REQUIRE_EQUAL(queue.empty(), false);
  // Drain queue.
  CAF_CHECK_EQUAL(fetch(1000), "1:7,2:8");
  CAF_REQUIRE_EQUAL(queue.empty(), true);
}

CAF_TEST(peek_all) {
  auto queue_to_string = [&] {
    std::string str;
    auto peek_fun = [&](const inode& x) {
      if (!str.empty())
        str += ", ";
      str += std::to_string(x.value);
    };
    queue.peek_all(peek_fun);
    return str;
  };
  make_queues();
  CAF_CHECK_EQUAL(queue_to_string(), "");
  queue.emplace_back(1);
  CAF_CHECK_EQUAL(queue_to_string(), "1");
  queue.emplace_back(2);
  CAF_CHECK_EQUAL(queue_to_string(), "1, 2");
  queue.emplace_back(3);
  // Lists are iterated in order and 3 is stored in the first queue for
  // `x mod 3 == 0` values.
  CAF_CHECK_EQUAL(queue_to_string(), "3, 1, 2");
  queue.emplace_back(4);
  CAF_CHECK_EQUAL(queue_to_string(), "3, 1, 4, 2");
}

CAF_TEST_FIXTURE_SCOPE_END()
