// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/policy/tcp.hpp"

#include <cstring>

#include "caf/io/network/native_socket.hpp"
#include "caf/logger.hpp"

#ifdef CAF_WINDOWS
#  include <winsock2.h>
#else
#  include <sys/socket.h>
#  include <sys/types.h>
#endif

using caf::io::network::is_error;
using caf::io::network::last_socket_error;
using caf::io::network::native_socket;
using caf::io::network::no_sigpipe_io_flag;
using caf::io::network::rw_state;
using caf::io::network::socket_error_as_string;
using caf::io::network::socket_size_type;

namespace caf::policy {

rw_state
tcp::read_some(size_t& result, native_socket fd, void* buf, size_t len) {
  CAF_LOG_TRACE(CAF_ARG(fd) << CAF_ARG(len));
  auto sres = ::recv(fd, reinterpret_cast<io::network::socket_recv_ptr>(buf),
                     len, no_sigpipe_io_flag);
  if (is_error(sres, true)) {
    // Make sure WSAGetLastError gets called immediately on Windows.
    auto err = last_socket_error();
    CAF_IGNORE_UNUSED(err);
    CAF_LOG_ERROR("recv failed:" << socket_error_as_string(err));
    return rw_state::failure;
  } else if (sres == 0) {
    // recv returns 0  when the peer has performed an orderly shutdown
    CAF_LOG_DEBUG("peer performed orderly shutdown" << CAF_ARG(fd));
    return rw_state::failure;
  }
  CAF_LOG_DEBUG(CAF_ARG(len) << CAF_ARG(fd) << CAF_ARG(sres));
  result = (sres > 0) ? static_cast<size_t>(sres) : 0;
  return rw_state::success;
}

rw_state
tcp::write_some(size_t& result, native_socket fd, const void* buf, size_t len) {
  CAF_LOG_TRACE(CAF_ARG(fd) << CAF_ARG(len));
  auto sres = ::send(fd, reinterpret_cast<io::network::socket_send_ptr>(buf),
                     len, no_sigpipe_io_flag);
  if (is_error(sres, true)) {
    // Make sure WSAGetLastError gets called immediately on Windows.
    auto err = last_socket_error();
    CAF_IGNORE_UNUSED(err);
    CAF_LOG_ERROR("send failed:" << socket_error_as_string(err));
    return rw_state::failure;
  }
  CAF_LOG_DEBUG(CAF_ARG(len) << CAF_ARG(fd) << CAF_ARG(sres));
  result = (sres > 0) ? static_cast<size_t>(sres) : 0;
  return rw_state::success;
}

bool tcp::try_accept(native_socket& result, native_socket fd) {
  using namespace io::network;
  CAF_LOG_TRACE(CAF_ARG(fd));
  sockaddr_storage addr;
  std::memset(&addr, 0, sizeof(addr));
  socket_size_type addrlen = sizeof(addr);
  result = ::accept(fd, reinterpret_cast<sockaddr*>(&addr), &addrlen);
  // note accept4 is better to avoid races in setting CLOEXEC (but not posix)
  if (result == invalid_native_socket) {
    auto err = last_socket_error();
    if (!would_block_or_temporarily_unavailable(err)) {
      CAF_LOG_ERROR("accept failed:" << socket_error_as_string(err));
      return false;
    }
  }
  child_process_inherit(result, false);
  CAF_LOG_DEBUG(CAF_ARG(fd) << CAF_ARG(result));
  return true;
}

} // namespace caf::policy
