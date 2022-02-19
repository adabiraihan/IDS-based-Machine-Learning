// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/config.hpp"
#include "caf/logger.hpp"
#include "caf/make_counted.hpp"
#include "caf/message_id.hpp"
#include "caf/policy/arg.hpp"
#include "caf/sec.hpp"
#include "caf/stream_manager.hpp"
#include "caf/stream_sink.hpp"
#include "caf/typed_message_view.hpp"

namespace caf::detail {

template <class Driver>
class stream_sink_impl : public Driver::sink_type {
public:
  using super = typename Driver::sink_type;

  using driver_type = Driver;

  using input_type = typename driver_type::input_type;

  template <class... Ts>
  stream_sink_impl(scheduled_actor* self, Ts&&... xs)
    : stream_manager(self), super(self), driver_(std::forward<Ts>(xs)...) {
    // nop
  }

  using super::handle;

  void handle(inbound_path*, downstream_msg::batch& x) override {
    CAF_LOG_TRACE(CAF_ARG(x));
    using vec_type = std::vector<input_type>;
    if (auto view = make_typed_message_view<vec_type>(x.xs)) {
      driver_.process(get<0>(view));
      return;
    }
    CAF_LOG_ERROR("received unexpected batch type (dropped)");
  }

  int32_t acquire_credit(inbound_path* path, int32_t desired) override {
    return driver_.acquire_credit(path, desired);
  }

protected:
  void finalize(const error& reason) override {
    driver_.finalize(reason);
  }

  driver_type driver_;
};

template <class Driver, class... Ts>
typename Driver::sink_ptr_type
make_stream_sink(scheduled_actor* self, Ts&&... xs) {
  using impl = stream_sink_impl<Driver>;
  return make_counted<impl>(self, std::forward<Ts>(xs)...);
}

} // namespace caf::detail
