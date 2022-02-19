// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/actor_registry.hpp"

#include <mutex>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

#include "caf/actor_system.hpp"
#include "caf/attachable.hpp"
#include "caf/detail/shared_spinlock.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/exit_reason.hpp"
#include "caf/locks.hpp"
#include "caf/logger.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/sec.hpp"
#include "caf/stateful_actor.hpp"

namespace caf {

namespace {

using exclusive_guard = unique_lock<detail::shared_spinlock>;
using shared_guard = shared_lock<detail::shared_spinlock>;

} // namespace

actor_registry::~actor_registry() {
  // nop
}

actor_registry::actor_registry(actor_system& sys) : system_(sys) {
  // nop
}

strong_actor_ptr actor_registry::get_impl(actor_id key) const {
  shared_guard guard(instances_mtx_);
  auto i = entries_.find(key);
  if (i != entries_.end())
    return i->second;
  CAF_LOG_DEBUG("key invalid, assume actor no longer exists:" << CAF_ARG(key));
  return nullptr;
}

void actor_registry::put_impl(actor_id key, strong_actor_ptr val) {
  CAF_LOG_TRACE(CAF_ARG(key));
  if (!val)
    return;
  { // lifetime scope of guard
    exclusive_guard guard(instances_mtx_);
    if (!entries_.emplace(key, val).second)
      return;
  }
  // attach functor without lock
  CAF_LOG_DEBUG("added actor:" << CAF_ARG(key));
  actor_registry* reg = this;
  val->get()->attach_functor([key, reg]() {
    reg->erase(key);
  });
}

void actor_registry::erase(actor_id key) {
  // Stores a reference to the actor we're going to remove. This guarantees
  // that we aren't releasing the last reference to an actor while erasing it.
  // Releasing the final ref can trigger the actor to call its cleanup function
  // that in turn calls this function and we can end up in a deadlock.
  strong_actor_ptr ref;
  { // Lifetime scope of guard.
    exclusive_guard guard{instances_mtx_};
    auto i = entries_.find(key);
    if (i != entries_.end()) {
      ref.swap(i->second);
      entries_.erase(i);
    }
  }
}

size_t actor_registry::inc_running() {
  return ++*system_.base_metrics().running_actors;
}

size_t actor_registry::running() const {
  return static_cast<size_t>(system_.base_metrics().running_actors->value());
}

size_t actor_registry::dec_running() {
  size_t new_val = --*system_.base_metrics().running_actors;
  if (new_val <= 1) {
    std::unique_lock<std::mutex> guard(running_mtx_);
    running_cv_.notify_all();
  }
  return new_val;
}

void actor_registry::await_running_count_equal(size_t expected) const {
  CAF_ASSERT(expected == 0 || expected == 1);
  CAF_LOG_TRACE(CAF_ARG(expected));
  std::unique_lock<std::mutex> guard{running_mtx_};
  while (running() != expected) {
    CAF_LOG_DEBUG(CAF_ARG(running()));
    running_cv_.wait(guard);
  }
}

strong_actor_ptr actor_registry::get_impl(const std::string& key) const {
  shared_guard guard{named_entries_mtx_};
  auto i = named_entries_.find(key);
  if (i == named_entries_.end())
    return nullptr;
  return i->second;
}

void actor_registry::put_impl(const std::string& key, strong_actor_ptr value) {
  if (value == nullptr) {
    erase(key);
    return;
  }
  exclusive_guard guard{named_entries_mtx_};
  named_entries_.emplace(std::move(key), std::move(value));
}

void actor_registry::erase(const std::string& key) {
  // Stores a reference to the actor we're going to remove for the same
  // reasoning as in erase(actor_id).
  strong_actor_ptr ref;
  { // Lifetime scope of guard.
    exclusive_guard guard{named_entries_mtx_};
    auto i = named_entries_.find(key);
    if (i != named_entries_.end()) {
      ref.swap(i->second);
      named_entries_.erase(i);
    }
  }
}

auto actor_registry::named_actors() const -> name_map {
  shared_guard guard{named_entries_mtx_};
  return named_entries_;
}

void actor_registry::start() {
  // nop
}

void actor_registry::stop() {
  {
    exclusive_guard guard{instances_mtx_};
    entries_.clear();
  }
  {
    exclusive_guard guard{named_entries_mtx_};
    named_entries_.clear();
  }
}

} // namespace caf
