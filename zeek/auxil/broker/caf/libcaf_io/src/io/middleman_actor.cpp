// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/io/middleman_actor.hpp"

#include <stdexcept>
#include <tuple>
#include <utility>

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/spawn_options.hpp"

#include "caf/io/middleman_actor_impl.hpp"

namespace caf::io {

middleman_actor make_middleman_actor(actor_system& sys, actor db) {
  return get_or(sys.config(), "caf.middleman.attach-utility-actors", false)
           ? sys.spawn<middleman_actor_impl, hidden>(std::move(db))
           : sys.spawn<middleman_actor_impl, detached + hidden>(std::move(db));
}

} // namespace caf::io
