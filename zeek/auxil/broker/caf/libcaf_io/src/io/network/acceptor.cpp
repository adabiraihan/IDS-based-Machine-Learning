// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/io/network/acceptor.hpp"

#include "caf/logger.hpp"

namespace caf::io::network {

acceptor::acceptor(default_multiplexer& backend_ref, native_socket sockfd)
  : event_handler(backend_ref, sockfd), sock_(invalid_native_socket) {
  // nop
}

void acceptor::start(acceptor_manager* mgr) {
  CAF_LOG_TRACE(CAF_ARG2("fd", fd_));
  CAF_ASSERT(mgr != nullptr);
  activate(mgr);
}

void acceptor::activate(acceptor_manager* mgr) {
  if (!mgr_) {
    mgr_.reset(mgr);
    event_handler::activate();
  }
}

void acceptor::removed_from_loop(operation op) {
  CAF_LOG_TRACE(CAF_ARG2("fd", fd_) << CAF_ARG(op));
  if (op == operation::read)
    mgr_.reset();
}

void acceptor::graceful_shutdown() {
  CAF_LOG_TRACE(CAF_ARG2("fd", fd_));
  // Ignore repeated calls.
  if (state_.shutting_down)
    return;
  state_.shutting_down = true;
  // Shutdown socket activity.
  shutdown_both(fd_);
}

} // namespace caf::io::network
