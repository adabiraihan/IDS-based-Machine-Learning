// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <functional>
#include <string>
#include <thread>

#include "caf/detail/io_export.hpp"
#include "caf/execution_unit.hpp"
#include "caf/expected.hpp"
#include "caf/extend.hpp"
#include "caf/io/accept_handle.hpp"
#include "caf/io/connection_handle.hpp"
#include "caf/io/fwd.hpp"
#include "caf/io/network/ip_endpoint.hpp"
#include "caf/io/network/native_socket.hpp"
#include "caf/io/network/protocol.hpp"
#include "caf/make_counted.hpp"
#include "caf/resumable.hpp"

namespace caf::io::network {

class multiplexer_backend;

/// Low-level backend for IO multiplexing.
class CAF_IO_EXPORT multiplexer : public execution_unit {
public:
  explicit multiplexer(actor_system* sys);

  /// Creates a new `scribe` from a native socket handle.
  /// @threadsafe
  virtual scribe_ptr new_scribe(native_socket fd) = 0;

  /// Tries to connect to `host` on given `port` and returns a `scribe` instance
  /// on success.
  /// @threadsafe
  virtual expected<scribe_ptr>
  new_tcp_scribe(const std::string& host, uint16_t port) = 0;

  /// Creates a new doorman from a native socket handle.
  /// @threadsafe
  virtual doorman_ptr new_doorman(native_socket fd) = 0;

  /// Tries to create an unbound TCP doorman bound to `port`, optionally
  /// accepting only connections from IP address `in`.
  /// @warning Do not call from outside the multiplexer's event loop.
  virtual expected<doorman_ptr>
  new_tcp_doorman(uint16_t port, const char* in = nullptr,
                  bool reuse_addr = false)
    = 0;

  /// Creates a new `datagram_servant` from a native socket handle.
  /// @threadsafe
  virtual datagram_servant_ptr new_datagram_servant(native_socket fd) = 0;

  virtual datagram_servant_ptr
  new_datagram_servant_for_endpoint(native_socket fd, const ip_endpoint& ep)
    = 0;

  /// Create a new `datagram_servant` to contact a remote endpoint `host` and
  /// `port`.
  /// @warning Do not call from outside the multiplexer's event loop.
  virtual expected<datagram_servant_ptr>
  new_remote_udp_endpoint(const std::string& host, uint16_t port) = 0;

  /// Create a new `datagram_servant` that receives datagrams on the local
  /// `port`, optionally only accepting connections from IP address `in`.
  /// @warning Do not call from outside the multiplexer's event loop.
  virtual expected<datagram_servant_ptr>
  new_local_udp_endpoint(uint16_t port, const char* in = nullptr,
                         bool reuse_addr = false)
    = 0;

  /// Simple wrapper for runnables
  class CAF_IO_EXPORT runnable : public resumable, public ref_counted {
  public:
    subtype_t subtype() const override;
    void intrusive_ptr_add_ref_impl() override;
    void intrusive_ptr_release_impl() override;
  };

  /// Makes sure the multipler does not exit its event loop until
  /// the destructor of `supervisor` has been called.
  class CAF_IO_EXPORT supervisor {
  public:
    virtual ~supervisor();
  };

  using supervisor_ptr = std::unique_ptr<supervisor>;

  /// Creates a supervisor to keep the event loop running.
  virtual supervisor_ptr make_supervisor() = 0;

  /// Creates an instance using the networking backend compiled with CAF.
  static std::unique_ptr<multiplexer> make(actor_system& sys);

  /// Exectutes all pending events without blocking.
  /// @returns `true` if at least one event was called, `false` otherwise.
  virtual bool try_run_once() = 0;

  /// Runs at least one event and blocks if needed.
  virtual void run_once() = 0;

  /// Runs events until all connection are closed.
  virtual void run() = 0;

  /// Invokes @p fun in the multiplexer's event loop, calling `fun()`
  /// immediately when called from inside the event loop.
  /// @threadsafe
  template <class F>
  void dispatch(F fun) {
    if (std::this_thread::get_id() == thread_id()) {
      fun();
      return;
    }
    post(std::move(fun));
  }

  /// Invokes @p fun in the multiplexer's event loop, forcing
  /// execution to be delayed when called from inside the event loop.
  /// @threadsafe
  template <class F>
  void post(F fun) {
    struct impl : runnable {
      F f;
      impl(F&& mf) : f(std::move(mf)) {
      }
      resume_result resume(execution_unit*, size_t) override {
        f();
        return done;
      }
    };
    exec_later(new impl(std::move(fun)));
  }

  /// Retrieves a pointer to the implementation or `nullptr` if CAF was
  /// compiled using the default backend.
  virtual multiplexer_backend* pimpl();

  const std::thread::id& thread_id() const {
    return tid_;
  }

  void thread_id(std::thread::id tid) {
    tid_ = std::move(tid);
  }

protected:
  /// Identifies the thread this multiplexer
  /// is running in. Must be set by the subclass.
  std::thread::id tid_;
};

using multiplexer_ptr = std::unique_ptr<multiplexer>;

} // namespace caf::io::network
