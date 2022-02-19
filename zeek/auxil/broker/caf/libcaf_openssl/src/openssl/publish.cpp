// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/openssl/publish.hpp"

#include <set>

#include "caf/actor_control_block.hpp"
#include "caf/actor_system.hpp"
#include "caf/expected.hpp"
#include "caf/function_view.hpp"
#include "caf/openssl/manager.hpp"

namespace caf::openssl {

expected<uint16_t> publish(actor_system& sys, const strong_actor_ptr& whom,
                           std::set<std::string>&& sigs, uint16_t port,
                           const char* cstr, bool ru) {
  CAF_LOG_TRACE(CAF_ARG(whom) << CAF_ARG(sigs) << CAF_ARG(port));
  CAF_ASSERT(whom != nullptr);
  std::string in;
  if (cstr != nullptr)
    in = cstr;
  auto f = make_function_view(sys.openssl_manager().actor_handle());
  return f(publish_atom_v, port, std::move(whom), std::move(sigs),
           std::move(in), ru);
}

} // namespace caf::openssl
