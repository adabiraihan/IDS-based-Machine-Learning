// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/size_based_credit_controller.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/serialized_size.hpp"
#include "caf/scheduled_actor.hpp"

namespace {

// Sample the first 10 batches when starting up.
constexpr int32_t initial_sample_size = 10;

} // namespace

namespace caf::detail {

size_based_credit_controller::size_based_credit_controller(local_actor* ptr)
  : self_(ptr), inspector_(ptr->system()) {
  namespace fallback = defaults::stream::size_policy;
  // Initialize from the config parameters.
  auto& cfg = ptr->system().config();
  if (auto section = get_if<settings>(&cfg, "caf.stream.size-based-policy")) {
    bytes_per_batch_ = get_or(*section, "bytes-per-batch",
                              fallback::bytes_per_batch);
    buffer_capacity_ = get_or(*section, "buffer-capacity",
                              fallback::buffer_capacity);
    calibration_interval_ = get_or(*section, "calibration-interval",
                                   fallback::calibration_interval);
    smoothing_factor_ = get_or(*section, "smoothing-factor",
                               fallback::smoothing_factor);
  } else {
    buffer_capacity_ = fallback::buffer_capacity;
    bytes_per_batch_ = fallback::bytes_per_batch;
    calibration_interval_ = fallback::calibration_interval;
    smoothing_factor_ = fallback::smoothing_factor;
  }
}

size_based_credit_controller::~size_based_credit_controller() {
  // nop
}

credit_controller::calibration size_based_credit_controller::init() {
  // Initially, we simply assume that the size of one element equals
  // bytes-per-batch.
  return {buffer_capacity_ / bytes_per_batch_, 1, initial_sample_size};
}

credit_controller::calibration size_based_credit_controller::calibrate() {
  CAF_ASSERT(sample_counter_ == 0);
  // Helper for truncating a 64-bit integer to a 32-bit integer with a minimum
  // value of 1.
  auto clamp_i32 = [](int64_t x) -> int32_t {
    static constexpr auto upper_bound = std::numeric_limits<int32_t>::max();
    if (x > upper_bound)
      return upper_bound;
    if (x <= 0)
      return 1;
    return static_cast<int32_t>(x);
  };
  if (!initializing_) {
    auto bpe = clamp_i32(sampled_total_size_ / sampled_elements_);
    bytes_per_element_ = static_cast<int32_t>(
      smoothing_factor_ * bpe // weighted current measurement
      + (1.0 - smoothing_factor_) * bytes_per_element_ // past values
    );
  } else {
    // After our first run, we continue with the actual sampling rate.
    initializing_ = false;
    sampling_rate_ = get_or(self_->config(),
                            "caf.stream.size-based-policy.sampling-rate",
                            defaults::stream::size_policy::sampling_rate);
    bytes_per_element_ = clamp_i32(sampled_total_size_ / sampled_elements_);
  }
  sampled_elements_ = 0;
  sampled_total_size_ = 0;
  return {
    clamp_i32(buffer_capacity_ / bytes_per_element_),
    clamp_i32(bytes_per_batch_ / bytes_per_element_),
    sampling_rate_ * calibration_interval_,
  };
}

} // namespace caf::detail
