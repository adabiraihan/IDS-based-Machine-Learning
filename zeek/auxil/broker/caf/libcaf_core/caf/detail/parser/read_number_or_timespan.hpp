// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include "caf/config.hpp"
#include "caf/detail/parser/chars.hpp"
#include "caf/detail/parser/is_char.hpp"
#include "caf/detail/parser/read_number.hpp"
#include "caf/detail/parser/read_timespan.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/none.hpp"
#include "caf/optional.hpp"
#include "caf/pec.hpp"
#include "caf/timestamp.hpp"
#include "caf/variant.hpp"

CAF_PUSH_UNUSED_LABEL_WARNING

#include "caf/detail/parser/fsm.hpp"

namespace caf::detail::parser {

/// Reads a number or a duration, i.e., on success produces an `int64_t`, a
/// `double`, or a `timespan`.
template <class State, class Consumer, class EnableRange = std::false_type>
void read_number_or_timespan(State& ps, Consumer& consumer,
                             EnableRange enable_range = {}) {
  using namespace std::chrono;
  struct interim_consumer {
    size_t invocations = 0;
    Consumer* outer = nullptr;
    variant<none_t, int64_t, double> interim;
    void value(int64_t x) {
      switch (++invocations) {
        case 1:
          interim = x;
          break;
        case 2:
          CAF_ASSERT(holds_alternative<int64_t>(interim));
          outer->value(get<int64_t>(interim));
          interim = none;
          [[fallthrough]];
        default:
          outer->value(x);
      }
    }
    void value(double x) {
      interim = x;
    }
  };
  interim_consumer ic;
  ic.outer = &consumer;
  auto has_int = [&] { return holds_alternative<int64_t>(ic.interim); };
  auto has_dbl = [&] { return holds_alternative<double>(ic.interim); };
  auto get_int = [&] { return get<int64_t>(ic.interim); };
  auto g = make_scope_guard([&] {
    if (ps.code <= pec::trailing_character) {
      if (has_dbl())
        consumer.value(get<double>(ic.interim));
      else if (has_int())
        consumer.value(get_int());
    }
  });
  static constexpr std::true_type enable_float = std::true_type{};
  // clang-format off
  start();
  state(init) {
    fsm_epsilon(read_number(ps, ic, enable_float, enable_range), has_number)
  }
  term_state(has_number) {
    epsilon_if(has_int(), has_integer)
    epsilon_if(has_dbl(), has_double)
  }
  term_state(has_double) {
    error_transition(pec::fractional_timespan, "unmsh")
  }
  term_state(has_integer) {
    fsm_epsilon(read_timespan(ps, consumer, get_int()),
                done, "unmsh", g.disable())
  }
  term_state(done) {
    // nop
  }
  fin();
  // clang-format on
}

} // namespace caf::detail::parser

#include "caf/detail/parser/fsm_undef.hpp"

CAF_POP_WARNINGS
