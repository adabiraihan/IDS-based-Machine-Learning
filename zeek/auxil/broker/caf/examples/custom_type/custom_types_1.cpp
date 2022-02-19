// Showcases how to add custom POD message types.

#include <cassert>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "caf/all.hpp"

// --(rst-type-id-block-begin)--
struct foo;
struct foo2;

CAF_BEGIN_TYPE_ID_BLOCK(custom_types_1, first_custom_type_id)

  CAF_ADD_TYPE_ID(custom_types_1, (foo))
  CAF_ADD_TYPE_ID(custom_types_1, (foo2))
  CAF_ADD_TYPE_ID(custom_types_1, (std::pair<int32_t, int32_t>) )

CAF_END_TYPE_ID_BLOCK(custom_types_1)
// --(rst-type-id-block-end)--

using std::cerr;
using std::cout;
using std::endl;
using std::vector;

using namespace caf;

// --(rst-foo-begin)--
struct foo {
  std::vector<int> a;
  int b;
};

template <class Inspector>
bool inspect(Inspector& f, foo& x) {
  return f.object(x).fields(f.field("a", x.a), f.field("b", x.b));
}
// --(rst-foo-end)--

// a pair of two ints
using foo_pair = std::pair<int, int>;

// another alias for pairs of two ints
using foo_pair2 = std::pair<int, int>;

// a struct with a nested container
struct foo2 {
  int a;
  vector<vector<double>> b;
};

// foo2 also needs to be serializable
template <class Inspector>
bool inspect(Inspector& f, foo2& x) {
  return f.object(x).fields(f.field("a", x.a), f.field("b", x.b));
}

// receives our custom message types
void testee(event_based_actor* self, size_t remaining) {
  auto set_next_behavior = [=] {
    if (remaining > 1)
      testee(self, remaining - 1);
    else
      self->quit();
  };
  self->become(
    // note: we sent a foo_pair2, but match on foo_pair
    // that works because both are aliases for std::pair<int, int>
    [=](const foo_pair& val) {
      aout(self) << "foo_pair" << deep_to_string(val) << endl;
      set_next_behavior();
    },
    [=](const foo& val) {
      aout(self) << deep_to_string(val) << endl;
      set_next_behavior();
    });
}

void caf_main(actor_system& sys) {
  // two variables for testing serialization
  foo2 f1;
  foo2 f2;
  // init some test data
  f1.a = 5;
  f1.b.resize(1);
  f1.b.back().push_back(42);
  // byte buffer
  binary_serializer::container_type buf;
  // write f1 to buffer
  binary_serializer sink{sys, buf};
  if (!sink.apply(f1)) {
    std::cerr << "*** failed to serialize foo2: " << to_string(sink.get_error())
              << '\n';
    return;
  }
  // read f2 back from buffer
  binary_deserializer source{sys, buf};
  if (!source.apply(f2)) {
    std::cerr << "*** failed to deserialize foo2: "
              << to_string(source.get_error()) << '\n';
    return;
  }
  // must be equal
  assert(deep_to_string(f1) == deep_to_string(f2));
  // spawn a testee that receives two messages of user-defined type
  auto t = sys.spawn(testee, 2u);
  scoped_actor self{sys};
  // send t a foo
  self->send(t, foo{std::vector<int>{1, 2, 3, 4}, 5});
  // send t a foo_pair2
  self->send(t, foo_pair2{3, 4});
}

CAF_MAIN(id_block::custom_types_1)
