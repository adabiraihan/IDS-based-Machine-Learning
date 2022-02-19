// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "caf/actor.hpp"
#include "caf/actor_cast.hpp"
#include "caf/actor_clock.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/downstream_msg.hpp"
#include "caf/fwd.hpp"
#include "caf/inbound_path.hpp"
#include "caf/ref_counted.hpp"
#include "caf/stream.hpp"
#include "caf/stream_slot.hpp"
#include "caf/timespan.hpp"
#include "caf/upstream_msg.hpp"

namespace caf {

/// Manages a single stream with any number of in- and outbound paths.
class CAF_CORE_EXPORT stream_manager : public ref_counted {
public:
  // -- constants --------------------------------------------------------------

  /// Configures whether this stream shall remain open even if no in- or
  /// outbound paths exist.
  static constexpr int is_continuous_flag = 0x0001;

  /// Denotes whether the stream is about to stop, only sending buffered
  /// elements.
  static constexpr int is_shutting_down_flag = 0x0002;

  /// Denotes whether the manager has stopped. Calling member functions such as
  /// stop() or abort() on it no longer has any effect.
  static constexpr int is_stopped_flag = 0x0004;

  // -- member types -----------------------------------------------------------

  using inbound_path_ptr = std::unique_ptr<inbound_path>;

  using inbound_paths_list = std::vector<inbound_path*>;

  /// Discrete point in time.
  using time_point = typename actor_clock::time_point;

  // -- constructors, destructors, and assignment operators --------------------

  stream_manager(scheduled_actor* selfptr,
                 stream_priority prio = stream_priority::normal);

  ~stream_manager() override;

  virtual void handle(inbound_path* from, downstream_msg::batch& x);

  virtual void handle(inbound_path* from, downstream_msg::close& x);

  virtual void handle(inbound_path* from, downstream_msg::forced_close& x);

  virtual bool handle(stream_slots, upstream_msg::ack_open& x);

  virtual void handle(stream_slots slots, upstream_msg::ack_batch& x);

  virtual void handle(stream_slots slots, upstream_msg::drop& x);

  virtual void handle(stream_slots slots, upstream_msg::forced_drop& x);

  /// Closes all output and input paths and sends the final result to the
  /// client.
  virtual void stop(error reason = none);

  /// Mark this stream as shutting down, only allowing flushing all related
  /// buffers of in- and outbound paths.
  virtual void shutdown();

  /// Pushes new data to downstream actors by sending batches. The amount of
  /// pushed data is limited by the available credit.
  virtual void push();

  /// Returns true if the handler is not able to process any further batches
  /// since it is unable to make progress sending on its own.
  virtual bool congested(const inbound_path& path) const noexcept;

  /// Sends a handshake to `dest`.
  /// @pre `dest != nullptr`
  virtual void
  deliver_handshake(response_promise& rp, stream_slot slot, message handshake);

  // -- implementation hooks for sources ---------------------------------------

  /// Tries to generate new messages for the stream. This member function does
  /// nothing on stages and sinks, but can trigger a source to produce more
  /// messages.
  virtual bool generate_messages();

  // -- interface functions ----------------------------------------------------

  /// Returns the manager for downstream communication.
  virtual downstream_manager& out() = 0;

  /// Returns the manager for downstream communication.
  const downstream_manager& out() const;

  /// Returns whether the manager has reached the end and can be discarded
  /// safely.
  virtual bool done() const = 0;

  /// Returns whether the manager cannot make any progress on its own at the
  /// moment. For example, a source is idle if it has filled its output buffer
  /// and there isn't any credit left.
  virtual bool idle() const noexcept = 0;

  /// Advances time.
  virtual void cycle_timeout(size_t cycle_nr);

  /// Acquires credit on an inbound path. The calculated credit to fill our
  /// queue for two cycles is `desired`, but the manager is allowed to return
  /// any non-negative value.
  virtual int32_t acquire_credit(inbound_path* path, int32_t desired);

  // -- input path management --------------------------------------------------

  /// Informs the manager that a new input path opens.
  /// @note The lifetime of inbound paths is managed by the downstream queue.
  ///       This function is called from the constructor of `inbound_path`.
  virtual void register_input_path(inbound_path* x);

  /// Informs the manager that an input path closes.
  /// @note The lifetime of inbound paths is managed by the downstream queue.
  ///       This function is called from the destructor of `inbound_path`.
  virtual void deregister_input_path(inbound_path* x) noexcept;

  /// Creates an inbound path to the current sender without any type checking.
  /// @pre `current_sender() != nullptr`
  /// @pre `out().terminal() == false`
  /// @private
  template <class In>
  stream_slot add_unchecked_inbound_path(stream<In> in) {
    auto path = std::make_unique<inbound_path>(this, in);
    return add_unchecked_inbound_path_impl(type_id_v<In>, std::move(path));
  }

  /// Removes an input path
  virtual void remove_input_path(stream_slot slot, error reason, bool silent);

  // -- properties -------------------------------------------------------------

  /// Returns whether this stream is neither shutting down nor has stopped.
  bool running() const noexcept {
    return getf(is_shutting_down_flag | is_stopped_flag) == 0;
  }

  /// Returns whether this stream remains open even if no in- or outbound paths
  /// exist. The default is `false`. Does not keep a source alive past the
  /// point where its driver returns `done() == true`.
  bool continuous() const noexcept {
    return getf(is_continuous_flag);
  }

  /// Sets whether this stream remains open even if no in- or outbound paths
  /// exist.
  void continuous(bool x) noexcept {
    if (running()) {
      if (x)
        setf(is_continuous_flag);
      else
        unsetf(is_continuous_flag);
    }
  }

  /// Returns the list of inbound paths.
  const inbound_paths_list& inbound_paths() const noexcept {
    return inbound_paths_;
  }

  /// Returns the inbound paths at slot `x`.
  inbound_path* get_inbound_path(stream_slot x) const noexcept;

  /// Queries whether all inbound paths are up-to-date and have non-zero
  /// credit. A sink is idle if this function returns `true`.
  bool inbound_paths_idle() const noexcept;

  /// Returns the parent actor.
  scheduled_actor* self() noexcept {
    return self_;
  }

  // -- output path management -------------------------------------------------

  /// Creates an outbound path to the current sender without any type checking.
  /// @pre `out().terminal() == false`
  /// @private
  template <class Out>
  outbound_stream_slot<Out> add_unchecked_outbound_path() {
    auto handshake = make_message(stream<Out>{});
    return add_unchecked_outbound_path_impl(std::move(handshake));
  }

  /// Creates an outbound path to the current sender without any type checking.
  /// @pre `out().terminal() == false`
  /// @private
  template <class Out, class... Ts>
  outbound_stream_slot<Out, detail::strip_and_convert_t<Ts>...>
  add_unchecked_outbound_path(std::tuple<Ts...> xs) {
    auto tk = std::make_tuple(stream<Out>{});
    auto handshake = make_message_from_tuple(std::tuple_cat(tk, std::move(xs)));
    return add_unchecked_outbound_path_impl(std::move(handshake));
  }

  /// Creates an outbound path to `next`, only checking whether the interface
  /// of `next` allows handshakes of type `Out`.
  /// @pre `next != nullptr`
  /// @pre `self()->pending_stream_managers_[slot] == this`
  /// @pre `out().terminal() == false`
  /// @private
  template <class Out, class Handle>
  outbound_stream_slot<Out> add_unchecked_outbound_path(Handle next) {
    // TODO: type-check whether `next` accepts our handshake
    auto handshake = make_message(stream<Out>{});
    auto hdl = actor_cast<strong_actor_ptr>(std::move(next));
    return add_unchecked_outbound_path_impl(std::move(hdl),
                                            std::move(handshake));
  }

  /// Creates an outbound path to `next`, only checking whether the interface
  /// of `next` allows handshakes of type `Out` with arguments `Ts...`.
  /// @pre `next != nullptr`
  /// @pre `self()->pending_stream_managers_[slot] == this`
  /// @pre `out().terminal() == false`
  /// @private
  template <class Out, class Handle, class... Ts>
  outbound_stream_slot<Out, detail::strip_and_convert_t<Ts>...>
  add_unchecked_outbound_path(const Handle& next, std::tuple<Ts...> xs) {
    // TODO: type-check whether `next` accepts our handshake
    auto tk = std::make_tuple(stream<Out>{});
    auto handshake = make_message_from_tuple(std::tuple_cat(tk, std::move(xs)));
    auto hdl = actor_cast<strong_actor_ptr>(std::move(next));
    return add_unchecked_outbound_path_impl(std::move(hdl),
                                            std::move(handshake));
  }

  /// Adds a new outbound path to `rp.next()`.
  /// @private
  stream_slot
  add_unchecked_outbound_path_impl(response_promise& rp, message handshake);

  /// Adds a new outbound path to `next`.
  /// @private
  stream_slot
  add_unchecked_outbound_path_impl(strong_actor_ptr next, message handshake);

  /// Adds a new outbound path to the current sender.
  /// @private
  stream_slot add_unchecked_outbound_path_impl(message handshake);

  // -- time management --------------------------------------------------------

  void tick(time_point now);

protected:
  // -- modifiers for self -----------------------------------------------------

  stream_slot assign_next_slot();

  stream_slot assign_next_pending_slot();

  // -- implementation hooks ---------------------------------------------------

  virtual void finalize(const error& reason);

  // -- implementation hooks for sinks -----------------------------------------

  /// Called when `in().closed()` changes to `true`. The default
  /// implementation does nothing.
  virtual void input_closed(error reason);

  // -- implementation hooks for sources ---------------------------------------

  /// Called whenever new credit becomes available. The default implementation
  /// logs an error (sources are expected to override this hook).
  virtual void downstream_demand(outbound_path* ptr, long demand);

  /// Called when `out().closed()` changes to `true`. The default
  /// implementation does nothing.
  virtual void output_closed(error reason);

  /// Adds the current sender as an inbound path.
  /// @pre Current message is an `open_stream_msg`.
  stream_slot add_unchecked_inbound_path_impl(type_id_t input_type,
                                              inbound_path_ptr path);

  // -- member variables -------------------------------------------------------

  /// Points to the parent actor.
  scheduled_actor* self_;

  /// Stores non-owning pointers to all input paths.
  inbound_paths_list inbound_paths_;

  /// Keeps track of pending handshakes.
  long pending_handshakes_;

  /// Configures the importance of outgoing traffic.
  stream_priority priority_;

  /// Stores individual flags, for continuous streaming or when shutting down.
  int flags_;

  /// Stores the maximum amount of time outbound paths should buffer elements
  /// before sending underful batches.
  timespan max_batch_delay_;

private:
  void setf(int flag) noexcept {
    auto x = flags_;
    flags_ = x | flag;
  }

  void unsetf(int flag) noexcept {
    auto x = flags_;
    flags_ = x & ~flag;
  }

  bool getf(int flag) const noexcept {
    return (flags_ & flag) != 0;
  }
};

/// A reference counting pointer to a `stream_manager`.
/// @relates stream_manager
using stream_manager_ptr = intrusive_ptr<stream_manager>;

} // namespace caf
