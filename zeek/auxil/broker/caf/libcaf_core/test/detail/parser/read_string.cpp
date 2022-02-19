// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE detail.parser.read_string

#include "caf/detail/parser/read_string.hpp"

#include "caf/test/unit_test.hpp"

#include <string>

#include "caf/expected.hpp"
#include "caf/parser_state.hpp"
#include "caf/string_view.hpp"
#include "caf/variant.hpp"

using namespace caf;

namespace {

struct string_parser_consumer {
  std::string x;
  inline void value(std::string y) {
    x = std::move(y);
  }
};

struct string_parser {
  expected<std::string> operator()(string_view str) {
    string_parser_consumer f;
    string_parser_state res{str.begin(), str.end()};
    detail::parser::read_string(res, f);
    if (res.code == pec::success)
      return f.x;
    return make_error(res.code, res.column, std::string{res.i, res.e});
  }
};

struct fixture {
  string_parser p;
};

// TODO: remove and use "..."s from the STL when switching to C++14
std::string operator"" _s(const char* str, size_t str_len) {
  std::string result;
  result.assign(str, str_len);
  return result;
}

} // namespace

CAF_TEST_FIXTURE_SCOPE(read_string_tests, fixture)

CAF_TEST(empty string) {
  CAF_CHECK_EQUAL(p(R"("")"), ""_s);
  CAF_CHECK_EQUAL(p(R"( "")"), ""_s);
  CAF_CHECK_EQUAL(p(R"(  "")"), ""_s);
  CAF_CHECK_EQUAL(p(R"("" )"), ""_s);
  CAF_CHECK_EQUAL(p(R"(""  )"), ""_s);
  CAF_CHECK_EQUAL(p(R"(  ""  )"), ""_s);
  CAF_CHECK_EQUAL(p("\t \"\" \t\t\t "), ""_s);
  CAF_CHECK_EQUAL(p(R"('')"), ""_s);
  CAF_CHECK_EQUAL(p(R"( '')"), ""_s);
  CAF_CHECK_EQUAL(p(R"(  '')"), ""_s);
  CAF_CHECK_EQUAL(p(R"('' )"), ""_s);
  CAF_CHECK_EQUAL(p(R"(''  )"), ""_s);
  CAF_CHECK_EQUAL(p(R"(  ''  )"), ""_s);
  CAF_CHECK_EQUAL(p("\t '' \t\t\t "), ""_s);
}

CAF_TEST(nonempty quoted string) {
  CAF_CHECK_EQUAL(p(R"("abc")"), "abc"_s);
  CAF_CHECK_EQUAL(p(R"("a b c")"), "a b c"_s);
  CAF_CHECK_EQUAL(p(R"(   "abcdefABCDEF"   )"), "abcdefABCDEF"_s);
  CAF_CHECK_EQUAL(p(R"('abc')"), "abc"_s);
  CAF_CHECK_EQUAL(p(R"('a b c')"), "a b c"_s);
  CAF_CHECK_EQUAL(p(R"(   'abcdefABCDEF'   )"), "abcdefABCDEF"_s);
}

CAF_TEST(quoted string with escaped characters) {
  CAF_CHECK_EQUAL(p(R"("a\tb\tc")"), "a\tb\tc"_s);
  CAF_CHECK_EQUAL(p(R"("a\nb\r\nc")"), "a\nb\r\nc"_s);
  CAF_CHECK_EQUAL(p(R"("a\\b")"), "a\\b"_s);
  CAF_CHECK_EQUAL(p("\"'hello' \\\"world\\\"\""), "'hello' \"world\""_s);
  CAF_CHECK_EQUAL(p(R"('a\tb\tc')"), "a\tb\tc"_s);
  CAF_CHECK_EQUAL(p(R"('a\nb\r\nc')"), "a\nb\r\nc"_s);
  CAF_CHECK_EQUAL(p(R"('a\\b')"), "a\\b"_s);
  CAF_CHECK_EQUAL(p(R"('\'hello\' "world"')"), "'hello' \"world\""_s);
}

CAF_TEST(unquoted strings) {
  CAF_CHECK_EQUAL(p(R"(foo)"), "foo"_s);
  CAF_CHECK_EQUAL(p(R"( foo )"), "foo"_s);
  CAF_CHECK_EQUAL(p(R"( 123 )"), "123"_s);
}

CAF_TEST(invalid strings) {
  CAF_CHECK_EQUAL(p(R"("abc)"), pec::unexpected_eof);
  CAF_CHECK_EQUAL(p(R"('abc)"), pec::unexpected_eof);
  CAF_CHECK_EQUAL(p("\"ab\nc\""), pec::unexpected_newline);
  CAF_CHECK_EQUAL(p("'ab\nc'"), pec::unexpected_newline);
  CAF_CHECK_EQUAL(p(R"("abc" def)"), pec::trailing_character);
  CAF_CHECK_EQUAL(p(R"('abc' def)"), pec::trailing_character);
  CAF_CHECK_EQUAL(p(R"( 123, )"), pec::trailing_character);
}

CAF_TEST_FIXTURE_SCOPE_END()
