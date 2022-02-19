// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cctype>
#include <cstdint>

#include "caf/fwd.hpp"
#include "caf/pec.hpp"
#include "caf/string_view.hpp"

namespace caf {

/// Stores all information necessary for implementing an FSM-based parser.
template <class Iterator, class Sentinel>
struct parser_state {
  /// Current position of the parser.
  Iterator i;

  /// End-of-input marker.
  Sentinel e;

  /// Current state of the parser.
  pec code;

  /// Current line in the input.
  int32_t line;

  /// Position in the current line.
  int32_t column;

  parser_state() noexcept : i(), e(), code(pec::success), line(1), column(1) {
    // nop
  }

  explicit parser_state(Iterator first) noexcept : parser_state() {
    i = first;
  }

  parser_state(Iterator first, Sentinel last) noexcept : parser_state() {
    i = first;
    e = last;
  }

  parser_state(const parser_state&) noexcept = default;

  parser_state& operator=(const parser_state&) noexcept = default;

  /// Returns the null terminator when reaching the end of the string,
  /// otherwise the next character.
  char next() noexcept {
    ++i;
    ++column;
    if (i != e) {
      auto c = *i;
      if (c == '\n') {
        ++line;
        column = 1;
      }
      return c;
    }
    return '\0';
  }

  /// Returns the null terminator if `i == e`, otherwise the current character.
  char current() const noexcept {
    return i != e ? *i : '\0';
  }

  /// Checks whether `i == e`.
  bool at_end() const noexcept {
    return i == e;
  }

  /// Skips any whitespaces characters in the input.
  void skip_whitespaces() noexcept {
    auto c = current();
    while (isspace(c))
      c = next();
  }

  /// Tries to read `x` as the next character, automatically skipping leading
  /// whitespaces.
  bool consume(char x) noexcept {
    skip_whitespaces();
    if (current() == x) {
      next();
      return true;
    }
    return false;
  }

  /// Consumes the next character if it satisfies given predicate, automatically
  /// skipping leading whitespaces.
  template <class Predicate>
  bool consume_if(Predicate predicate) noexcept {
    skip_whitespaces();
    if (predicate(current())) {
      next();
      return true;
    }
    return false;
  }

  /// Tries to read `x` as the next character without automatically skipping
  /// leading whitespaces.
  bool consume_strict(char x) noexcept {
    if (current() == x) {
      next();
      return true;
    }
    return false;
  }

  /// Consumes the next character if it satisfies given predicate without
  /// automatically skipping leading whitespaces.
  template <class Predicate>
  bool consume_strict_if(Predicate predicate) noexcept {
    if (predicate(current())) {
      next();
      return true;
    }
    return false;
  }
};

/// Returns an error object from the current code in `ps` as well as its
/// current position.
template <class Iterator, class Sentinel, class... Ts>
auto make_error(const parser_state<Iterator, Sentinel>& ps, Ts&&... xs)
  -> decltype(make_error(ps.code, ps.line, ps.column)) {
  if (ps.code == pec::success)
    return {};
  return make_error(ps.code, ps.line, ps.column, std::forward<Ts>(xs)...);
}

/// Specialization for parsers operating on string views.
using string_parser_state = parser_state<string_view::iterator>;

} // namespace caf
