#include "broker/detail/master_resolver.hh"

#include <string>
#include <utility>
#include <vector>

#include <caf/actor.hpp>
#include <caf/behavior.hpp>
#include <caf/event_based_actor.hpp>
#include <caf/stateful_actor.hpp>

#include "broker/atoms.hh"
#include "broker/error.hh"
#include "broker/logger.hh"

namespace broker {
namespace detail {

caf::behavior master_resolver(master_resolver_actor* self) {
  self->set_error_handler([=](error&) {
    if (--self->state.remaining_responses == 0) {
      BROKER_DEBUG("resolver failed to find a master");
      self->send(self->state.who_asked, atom::master_v,
                 make_error(ec::no_such_master, "no master on peers"));
      self->quit();
    }
  });
  return {
    [=](const std::vector<caf::actor>& peers, const std::string& name,
        caf::actor& who_asked) {
      BROKER_DEBUG("resolver starts looking for:" << name);
      for (auto& peer : peers)
        self->send(peer, atom::store_v, atom::master_v,
                   atom::get_v, name);

      self->state.remaining_responses = peers.size();
      self->state.who_asked = std::move(who_asked);
    },
    [=](caf::actor& master) {
      BROKER_DEBUG("resolver found master:" << master);
      self->send(self->state.who_asked, atom::master_v,
                 std::move(master));
      self->quit();
    }
  };
}

} // namespace detail
} // namespace broker
