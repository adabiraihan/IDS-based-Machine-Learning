// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <tuple>

#include "caf/delegated.hpp"
#include "caf/detail/implicit_conversions.hpp"
#include "caf/fwd.hpp"
#include "caf/stream.hpp"
#include "caf/stream_slot.hpp"
#include "caf/stream_stage.hpp"

namespace caf {

/// Returns a stream stage with the slot IDs of its first in- and outbound
/// paths.
template <class In, class DownstreamManager, class... Ts>
class make_stage_result
  : public delegated<stream<typename DownstreamManager::output_type>, Ts...> {
public:
  // -- member types -----------------------------------------------------------

  /// Type of a single element.
  using input_type = In;

  /// Type of a single element.
  using output_type = typename DownstreamManager::output_type;

  /// Fully typed stream manager as returned by `make_source`.
  using stage_type = stream_stage<In, DownstreamManager>;

  /// Pointer to a fully typed stream manager.
  using stage_ptr_type = intrusive_ptr<stage_type>;

  /// The return type for `scheduled_actor::make_stage`.
  using stream_type = stream<output_type>;

  /// Type of user-defined handshake arguments.
  using handshake_arguments = std::tuple<Ts...>;

  // -- constructors, destructors, and assignment operators --------------------

  make_stage_result() noexcept : inbound_slot_(0), outbound_slot_(0) {
    // nop
  }

  make_stage_result(stream_slot in, stream_slot out, stage_ptr_type ptr)
    : inbound_slot_(in), outbound_slot_(out), ptr_(std::move(ptr)) {
    // nop
  }

  make_stage_result(make_stage_result&&) = default;
  make_stage_result(const make_stage_result&) = default;
  make_stage_result& operator=(make_stage_result&&) = default;
  make_stage_result& operator=(const make_stage_result&) = default;

  // -- properties -------------------------------------------------------------

  stream_slot inbound_slot() const noexcept {
    return inbound_slot_;
  }

  stream_slot outbound_slot() const noexcept {
    return outbound_slot_;
  }

  stage_ptr_type& ptr() noexcept {
    return ptr_;
  }

  const stage_ptr_type& ptr() const noexcept {
    return ptr_;
  }

private:
  stream_slot inbound_slot_;
  stream_slot outbound_slot_;
  stage_ptr_type ptr_;
};

/// Helper type for defining a `make_stage_result` from a downstream manager
/// plus additional handshake types. Hardwires `message` as result type.
template <class In, class DownstreamManager, class... Ts>
using make_stage_result_t
  = make_stage_result<In, DownstreamManager,
                      detail::strip_and_convert_t<Ts>...>;

} // namespace caf
