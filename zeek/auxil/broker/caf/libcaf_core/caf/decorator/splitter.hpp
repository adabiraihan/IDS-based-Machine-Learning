// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <vector>

#include "caf/actor_addr.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/monitorable_actor.hpp"

namespace caf::decorator {

/// An actor decorator implementing "dot operator"-like compositions,
/// i.e., `f.g(x) = f(g(x))`. Composed actors are hidden actors.
/// A composed actor exits when either of its constituent actors exits;
/// Constituent actors have no dependency on the composed actor
/// by default, and exit of a composed actor has no effect on its
/// constituent actors. A composed actor is hosted on the same actor
/// system and node as `g`, the first actor on the forwarding chain.
class CAF_CORE_EXPORT splitter : public monitorable_actor {
public:
  using message_types_set = std::set<std::string>;

  splitter(std::vector<strong_actor_ptr> workers, message_types_set msg_types);

  // non-system messages are processed and then forwarded;
  // system messages are handled and consumed on the spot;
  // in either case, the processing is done synchronously
  void enqueue(mailbox_element_ptr what, execution_unit* context) override;

  message_types_set message_types() const override;

private:
  const size_t num_workers;
  std::vector<strong_actor_ptr> workers_;
  message_types_set msg_types_;
};

} // namespace caf::decorator
