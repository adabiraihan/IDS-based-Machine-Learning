// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE detail.parser.read_number_or_timespan

#include "caf/detail/parser/read_number_or_timespan.hpp"

#include "caf/test/unit_test.hpp"

#include <string>

#include "caf/parser_state.hpp"
#include "caf/string_view.hpp"
#include "caf/variant.hpp"

using namespace caf;
using namespace std::chrono;

namespace {

struct number_or_timespan_parser_consumer {
  variant<int64_t, double, timespan> x;
  template <class T>
  void value(T y) {
    x = y;
  }
};

struct res_t {
  variant<pec, double, int64_t, timespan> val;
  template <class T>
  res_t(T&& x) : val(std::forward<T>(x)) {
    // nop
  }
};

std::string to_string(const res_t& x) {
  return deep_to_string(x.val);
}

bool operator==(const res_t& x, const res_t& y) {
  if (x.val.index() != y.val.index())
    return false;
  // Implements a safe equal comparison for double.
  caf::test::equal_to f;
  using caf::get;
  using caf::holds_alternative;
  if (holds_alternative<pec>(x.val))
    return f(get<pec>(x.val), get<pec>(y.val));
  if (holds_alternative<double>(x.val))
    return f(get<double>(x.val), get<double>(y.val));
  if (holds_alternative<int64_t>(x.val))
    return f(get<int64_t>(x.val), get<int64_t>(y.val));
  return f(get<timespan>(x.val), get<timespan>(y.val));
}

struct number_or_timespan_parser {
  res_t operator()(string_view str) {
    number_or_timespan_parser_consumer f;
    string_parser_state res{str.begin(), str.end()};
    detail::parser::read_number_or_timespan(res, f);
    if (res.code == pec::success)
      return f.x;
    return res.code;
  }
};

struct fixture {
  number_or_timespan_parser p;
};

template <class T>
typename std::enable_if<std::is_integral<T>::value, res_t>::type res(T x) {
  return {static_cast<int64_t>(x)};
}

template <class T>
typename std::enable_if<std::is_floating_point<T>::value, res_t>::type
res(T x) {
  return {static_cast<double>(x)};
}

template <class Rep, class Period>
res_t res(std::chrono::duration<Rep, Period> x) {
  return std::chrono::duration_cast<timespan>(x);
}

} // namespace

CAF_TEST_FIXTURE_SCOPE(read_number_or_timespan_tests, fixture)

CAF_TEST(valid numbers and timespans) {
  CAF_CHECK_EQUAL(p("123"), res(123));
  CAF_CHECK_EQUAL(p("123.456"), res(123.456));
  CAF_CHECK_EQUAL(p("123s"), res(seconds(123)));
  CAF_CHECK_EQUAL(p("123ns"), res(nanoseconds(123)));
  CAF_CHECK_EQUAL(p("123ms"), res(milliseconds(123)));
  CAF_CHECK_EQUAL(p("123us"), res(microseconds(123)));
  CAF_CHECK_EQUAL(p("123min"), res(minutes(123)));
}

CAF_TEST(invalid timespans) {
  CAF_CHECK_EQUAL(p("12.3s"), pec::fractional_timespan);
  CAF_CHECK_EQUAL(p("12.3n"), pec::fractional_timespan);
  CAF_CHECK_EQUAL(p("12.3ns"), pec::fractional_timespan);
  CAF_CHECK_EQUAL(p("12.3m"), pec::fractional_timespan);
  CAF_CHECK_EQUAL(p("12.3ms"), pec::fractional_timespan);
  CAF_CHECK_EQUAL(p("12.3n"), pec::fractional_timespan);
  CAF_CHECK_EQUAL(p("12.3ns"), pec::fractional_timespan);
  CAF_CHECK_EQUAL(p("12.3mi"), pec::fractional_timespan);
  CAF_CHECK_EQUAL(p("12.3min"), pec::fractional_timespan);
  CAF_CHECK_EQUAL(p("123ss"), pec::trailing_character);
  CAF_CHECK_EQUAL(p("123m"), pec::unexpected_eof);
  CAF_CHECK_EQUAL(p("123mi"), pec::unexpected_eof);
  CAF_CHECK_EQUAL(p("123u"), pec::unexpected_eof);
  CAF_CHECK_EQUAL(p("123n"), pec::unexpected_eof);
}

CAF_TEST_FIXTURE_SCOPE_END()
