// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/io/network/scribe_impl.hpp"

#include <algorithm>

#include "caf/logger.hpp"

#include "caf/io/network/default_multiplexer.hpp"

namespace caf::io::network {

scribe_impl::scribe_impl(default_multiplexer& mx, native_socket sockfd)
  : scribe(network::conn_hdl_from_socket(sockfd)),
    launched_(false),
    stream_(mx, sockfd) {
  // nop
}

void scribe_impl::configure_read(receive_policy::config config) {
  CAF_LOG_TRACE("");
  stream_.configure_read(config);
  if (!launched_)
    launch();
}

void scribe_impl::ack_writes(bool enable) {
  CAF_LOG_TRACE(CAF_ARG(enable));
  stream_.ack_writes(enable);
}

byte_buffer& scribe_impl::wr_buf() {
  return stream_.wr_buf();
}

byte_buffer& scribe_impl::rd_buf() {
  return stream_.rd_buf();
}

void scribe_impl::graceful_shutdown() {
  CAF_LOG_TRACE("");
  stream_.graceful_shutdown();
  detach(&stream_.backend(), false);
}

void scribe_impl::flush() {
  CAF_LOG_TRACE("");
  stream_.flush(this);
}

std::string scribe_impl::addr() const {
  auto x = remote_addr_of_fd(stream_.fd());
  if (!x)
    return "";
  return *x;
}

uint16_t scribe_impl::port() const {
  auto x = remote_port_of_fd(stream_.fd());
  if (!x)
    return 0;
  return *x;
}

void scribe_impl::launch() {
  CAF_LOG_TRACE("");
  CAF_ASSERT(!launched_);
  launched_ = true;
  stream_.start(this);
}

void scribe_impl::add_to_loop() {
  stream_.activate(this);
}

void scribe_impl::remove_from_loop() {
  stream_.passivate();
}

} // namespace caf::io::network
