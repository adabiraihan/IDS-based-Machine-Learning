// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/simple_actor_clock.hpp"

#include "caf/actor_cast.hpp"
#include "caf/sec.hpp"
#include "caf/system_messages.hpp"

namespace caf::detail {

simple_actor_clock::event::~event() {
  // nop
}

void simple_actor_clock::set_ordinary_timeout(time_point t,
                                              abstract_actor* self,
                                              std::string type, uint64_t id) {
  new_schedule_entry<ordinary_timeout>(t, self->ctrl(), type, id);
}

void simple_actor_clock::set_multi_timeout(time_point t, abstract_actor* self,
                                           std::string type, uint64_t id) {
  new_schedule_entry<multi_timeout>(t, self->ctrl(), type, id);
}

void simple_actor_clock::set_request_timeout(time_point t, abstract_actor* self,
                                             message_id id) {
  new_schedule_entry<request_timeout>(t, self->ctrl(), id);
}

void simple_actor_clock::cancel_ordinary_timeout(abstract_actor* self,
                                                 std::string type) {
  ordinary_timeout_cancellation tmp{self->id(), std::move(type)};
  handle(tmp);
}

void simple_actor_clock::cancel_request_timeout(abstract_actor* self,
                                                message_id id) {
  request_timeout_cancellation tmp{self->id(), id};
  handle(tmp);
}

void simple_actor_clock::cancel_timeouts(abstract_actor* self) {
  auto range = actor_lookup_.equal_range(self->id());
  if (range.first == range.second)
    return;
  for (auto i = range.first; i != range.second; ++i)
    schedule_.erase(i->second);
  actor_lookup_.erase(range.first, range.second);
}

void simple_actor_clock::schedule_message(time_point t,
                                          strong_actor_ptr receiver,
                                          mailbox_element_ptr content) {
  new_schedule_entry<actor_msg>(t, std::move(receiver), std::move(content));
}

void simple_actor_clock::schedule_message(time_point t, group target,
                                          strong_actor_ptr sender,
                                          message content) {
  new_schedule_entry<group_msg>(t, std::move(target), std::move(sender),
                                std::move(content));
}

void simple_actor_clock::cancel_all() {
  actor_lookup_.clear();
  schedule_.clear();
}

void simple_actor_clock::ship(delayed_event& x) {
  switch (x.subtype) {
    case ordinary_timeout_type: {
      auto& dref = static_cast<ordinary_timeout&>(x);
      auto& self = dref.self;
      self->get()->eq_impl(make_message_id(), self, nullptr,
                           timeout_msg{dref.type, dref.id});
      break;
    }
    case multi_timeout_type: {
      auto& dref = static_cast<multi_timeout&>(x);
      auto& self = dref.self;
      self->get()->eq_impl(make_message_id(), self, nullptr,
                           timeout_msg{dref.type, dref.id});
      break;
    }
    case request_timeout_type: {
      auto& dref = static_cast<request_timeout&>(x);
      auto& self = dref.self;
      self->get()->eq_impl(dref.id, self, nullptr, sec::request_timeout);
      break;
    }
    case actor_msg_type: {
      auto& dref = static_cast<actor_msg&>(x);
      dref.receiver->enqueue(std::move(dref.content), nullptr);
      break;
    }
    case group_msg_type: {
      auto& dref = static_cast<group_msg&>(x);
      auto dst = dref.target->get();
      if (dst)
        dst->enqueue(std::move(dref.sender), make_message_id(),
                     std::move(dref.content), nullptr);
      break;
    }
    default:
      break;
  }
}

void simple_actor_clock::handle(const ordinary_timeout_cancellation& x) {
  auto pred = [&](const actor_lookup_map::value_type& kvp) {
    auto& y = *kvp.second->second;
    return y.subtype == ordinary_timeout_type
           && x.type == static_cast<const ordinary_timeout&>(y).type;
  };
  cancel(x.aid, pred);
}

void simple_actor_clock::handle(const multi_timeout_cancellation& x) {
  auto pred = [&](const actor_lookup_map::value_type& kvp) {
    auto& y = *kvp.second->second;
    if (y.subtype != multi_timeout_type)
      return false;
    auto& dref = static_cast<const multi_timeout&>(y);
    return x.type == dref.type && x.id == dref.id;
  };
  cancel(x.aid, pred);
}

void simple_actor_clock::handle(const request_timeout_cancellation& x) {
  auto pred = [&](const actor_lookup_map::value_type& kvp) {
    auto& y = *kvp.second->second;
    return y.subtype == request_timeout_type
           && x.id == static_cast<const request_timeout&>(y).id;
  };
  cancel(x.aid, pred);
}

void simple_actor_clock::handle(const timeouts_cancellation& x) {
  auto range = actor_lookup_.equal_range(x.aid);
  if (range.first == range.second)
    return;
  for (auto i = range.first; i != range.second; ++i)
    schedule_.erase(i->second);
  actor_lookup_.erase(range.first, range.second);
}

size_t simple_actor_clock::trigger_expired_timeouts() {
  size_t result = 0;
  auto t = now();
  auto i = schedule_.begin();
  auto e = schedule_.end();
  while (i != e && i->first <= t) {
    auto ptr = std::move(i->second);
    auto backlink = ptr->backlink;
    if (backlink != actor_lookup_.end())
      actor_lookup_.erase(backlink);
    i = schedule_.erase(i);
    ship(*ptr);
    ++result;
  }
  return result;
}

void simple_actor_clock::add_schedule_entry(
  time_point t, std::unique_ptr<ordinary_timeout> x) {
  auto aid = x->self->id();
  auto type = x->type;
  auto pred = [&](const actor_lookup_map::value_type& kvp) {
    auto& y = *kvp.second->second;
    return y.subtype == ordinary_timeout_type
           && static_cast<const ordinary_timeout&>(y).type == type;
  };
  auto i = lookup(aid, pred);
  if (i != actor_lookup_.end()) {
    schedule_.erase(i->second);
    i->second = schedule_.emplace(t, std::move(x));
  } else {
    auto j = schedule_.emplace(t, std::move(x));
    i = actor_lookup_.emplace(aid, j);
  }
  i->second->second->backlink = i;
}

} // namespace caf::detail
