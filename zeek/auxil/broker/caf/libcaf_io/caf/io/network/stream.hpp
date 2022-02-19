// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <vector>

#include "caf/byte_buffer.hpp"
#include "caf/detail/io_export.hpp"
#include "caf/io/fwd.hpp"
#include "caf/io/network/event_handler.hpp"
#include "caf/io/network/rw_state.hpp"
#include "caf/io/network/stream_manager.hpp"
#include "caf/io/receive_policy.hpp"
#include "caf/logger.hpp"
#include "caf/ref_counted.hpp"

namespace caf::io::network {

/// A stream capable of both reading and writing. The stream's input
/// data is forwarded to its {@link stream_manager manager}.
class CAF_IO_EXPORT stream : public event_handler {
public:
  /// A smart pointer to a stream manager.
  using manager_ptr = intrusive_ptr<stream_manager>;

  stream(default_multiplexer& backend_ref, native_socket sockfd);

  /// Starts reading data from the socket, forwarding incoming data to `mgr`.
  void start(stream_manager* mgr);

  /// Activates the stream.
  void activate(stream_manager* mgr);

  /// Configures how much data will be provided for the next `consume` callback.
  /// @warning Must not be called outside the IO multiplexers event loop
  ///          once the stream has been started.
  void configure_read(receive_policy::config config);

  /// Copies data to the write buffer.
  /// @warning Not thread safe.
  void write(const void* buf, size_t num_bytes);

  /// Returns the write buffer of this stream.
  /// @warning Must not be modified outside the IO multiplexers event loop
  ///          once the stream has been started.
  byte_buffer& wr_buf() {
    return wr_offline_buf_;
  }

  /// Returns the read buffer of this stream.
  /// @warning Must not be modified outside the IO multiplexers event loop
  ///          once the stream has been started.
  byte_buffer& rd_buf() {
    return rd_buf_;
  }

  /// Sends the content of the write buffer, calling the `io_failure`
  /// member function of `mgr` in case of an error.
  /// @warning Must not be called outside the IO multiplexers event loop
  ///          once the stream has been started.
  void flush(const manager_ptr& mgr);

  void removed_from_loop(operation op) override;

  void graceful_shutdown() override;

  /// Forces this stream to subscribe to write events if no data is in the
  /// write buffer.
  void force_empty_write(const manager_ptr& mgr);

protected:
  template <class Policy>
  void handle_event_impl(io::network::operation op, Policy& policy) {
    CAF_LOG_TRACE(CAF_ARG(op));
    switch (op) {
      case io::network::operation::read: {
        // Loop until an error occurs or we have nothing more to read
        // or until we have handled `mcr` reads.
        size_t rb = 0;
        auto threshold = [&] {
          CAF_ASSERT(read_threshold_ >= collected_);
          return read_threshold_ - collected_;
        };
        size_t reads = 0;
        while (reads < max_consecutive_reads_
               || policy.must_read_more(fd(), threshold())) {
          auto res = policy.read_some(rb, fd(), rd_buf_.data() + collected_,
                                      rd_buf_.size() - collected_);
          if (!handle_read_result(res, rb))
            return;
          ++reads;
        }
        break;
      }
      case io::network::operation::write: {
        size_t wb; // Written bytes.
        auto res = policy.write_some(wb, fd(), wr_buf_.data() + written_,
                                     wr_buf_.size() - written_);
        handle_write_result(res, wb);
        break;
      }
      case operation::propagate_error:
        handle_error_propagation();
        break;
    }
  }

private:
  void prepare_next_read();

  void prepare_next_write();

  bool handle_read_result(rw_state read_result, size_t rb);

  void handle_write_result(rw_state write_result, size_t wb);

  void handle_error_propagation();

  /// Initiates a graceful shutdown of the connection by sending FIN on the TCP
  /// connection.
  void send_fin();

  size_t max_consecutive_reads_;

  // State for reading.
  manager_ptr reader_;
  size_t read_threshold_;
  size_t collected_;
  size_t max_;
  byte_buffer rd_buf_;

  // State for writing.
  manager_ptr writer_;
  size_t written_;
  byte_buffer wr_buf_;
  byte_buffer wr_offline_buf_;
  bool wr_op_backoff_;
};

} // namespace caf::io::network
