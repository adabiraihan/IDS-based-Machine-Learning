// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/openssl/manager.hpp"

CAF_PUSH_WARNINGS
#include <openssl/err.h>
#include <openssl/ssl.h>
CAF_POP_WARNINGS

#include <mutex>
#include <vector>

#include "caf/actor_control_block.hpp"
#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/expected.hpp"
#include "caf/raise_error.hpp"
#include "caf/scoped_actor.hpp"

#include "caf/io/basp_broker.hpp"
#include "caf/io/middleman.hpp"
#include "caf/io/network/default_multiplexer.hpp"

#include "caf/openssl/middleman_actor.hpp"

#if OPENSSL_VERSION_NUMBER < 0x10100000L
struct CRYPTO_dynlock_value {
  std::mutex mtx;
};

namespace {

int init_count = 0;

std::mutex init_mutex;

std::vector<std::mutex> mutexes;

void locking_function(int mode, int n, const char*, int) {
  if (mode & CRYPTO_LOCK)
    mutexes[static_cast<size_t>(n)].lock();
  else
    mutexes[static_cast<size_t>(n)].unlock();
}

CRYPTO_dynlock_value* dynlock_create(const char*, int) {
  return new CRYPTO_dynlock_value;
}

void dynlock_lock(int mode, CRYPTO_dynlock_value* dynlock, const char*, int) {
  if (mode & CRYPTO_LOCK)
    dynlock->mtx.lock();
  else
    dynlock->mtx.unlock();
}

void dynlock_destroy(CRYPTO_dynlock_value* dynlock, const char*, int) {
  delete dynlock;
}

} // namespace

#endif

namespace caf::openssl {

manager::~manager() {
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  std::lock_guard<std::mutex> lock{init_mutex};
  --init_count;
  if (init_count == 0) {
    CRYPTO_set_locking_callback(nullptr);
    CRYPTO_set_dynlock_create_callback(nullptr);
    CRYPTO_set_dynlock_lock_callback(nullptr);
    CRYPTO_set_dynlock_destroy_callback(nullptr);
    mutexes = std::vector<std::mutex>(0);
  }
#endif
}

void manager::start() {
  CAF_LOG_TRACE("");
  manager_ = make_middleman_actor(
    system(), system().middleman().named_broker<io::basp_broker>("BASP"));
}

void manager::stop() {
  CAF_LOG_TRACE("");
  scoped_actor self{system(), true};
  self->send_exit(manager_, exit_reason::kill);
  if (!get_or(config(), "caf.middleman.attach-utility-actors", false))
    self->wait_for(manager_);
  manager_ = nullptr;
}

void manager::init(actor_system_config&) {
  ERR_load_crypto_strings();
  OPENSSL_add_all_algorithms_conf();
  SSL_library_init();
  SSL_load_error_strings();
  if (authentication_enabled()) {
    if (system().config().openssl_certificate.empty())
      CAF_RAISE_ERROR("No certificate configured for SSL endpoint");
    if (system().config().openssl_key.empty())
      CAF_RAISE_ERROR("No private key configured for SSL endpoint");
  }
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  std::lock_guard<std::mutex> lock{init_mutex};
  ++init_count;
  if (init_count == 1) {
    mutexes = std::vector<std::mutex>{static_cast<size_t>(CRYPTO_num_locks())};
    CRYPTO_set_locking_callback(locking_function);
    CRYPTO_set_dynlock_create_callback(dynlock_create);
    CRYPTO_set_dynlock_lock_callback(dynlock_lock);
    CRYPTO_set_dynlock_destroy_callback(dynlock_destroy);
    // OpenSSL's default thread ID callback should work, so don't set our own.
  }
#endif
}

actor_system::module::id_t manager::id() const {
  return openssl_manager;
}

void* manager::subtype_ptr() {
  return this;
}

bool manager::authentication_enabled() {
  auto& cfg = system().config();
  return !cfg.openssl_certificate.empty() || !cfg.openssl_key.empty()
         || !cfg.openssl_passphrase.empty() || !cfg.openssl_capath.empty()
         || !cfg.openssl_cafile.empty();
}

void manager::add_module_options(actor_system_config& cfg) {
  config_option_adder(cfg.custom_options(), "caf.openssl")
    .add<std::string>(cfg.openssl_certificate, "certificate",
                      "path to the PEM-formatted certificate file")
    .add<std::string>(cfg.openssl_key, "key",
                      "path to the private key file for this node")
    .add<std::string>(cfg.openssl_passphrase, "passphrase",
                      "passphrase to decrypt the private key")
    .add<std::string>(
      cfg.openssl_capath, "capath",
      "path to an OpenSSL-style directory of trusted certificates")
    .add<std::string>(
      cfg.openssl_cafile, "cafile",
      "path to a file of concatenated PEM-formatted certificates");
}

actor_system::module* manager::make(actor_system& sys, detail::type_list<>) {
  if (!sys.has_middleman())
    CAF_RAISE_ERROR("Cannot start OpenSSL module without middleman.");
  auto ptr = &sys.middleman().backend();
  if (dynamic_cast<io::network::default_multiplexer*>(ptr) == nullptr)
    CAF_RAISE_ERROR("Cannot start OpenSSL module without default backend.");
  return new manager(sys);
}

void manager::init_global_meta_objects() {
  // nop
}

manager::manager(actor_system& sys) : system_(sys) {
  // nop
}

} // namespace caf::openssl
