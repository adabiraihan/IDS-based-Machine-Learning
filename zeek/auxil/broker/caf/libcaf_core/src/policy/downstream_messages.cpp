// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/policy/downstream_messages.hpp"

#include "caf/downstream_msg.hpp"
#include "caf/inbound_path.hpp"
#include "caf/logger.hpp"
#include "caf/stream_manager.hpp"

namespace caf::policy {

auto downstream_messages::nested::task_size(
  const downstream_msg_batch& batch) noexcept -> task_size_type {
  CAF_ASSERT(batch.xs_size > 0);
  return batch.xs_size;
}

auto downstream_messages::nested::task_size(const mailbox_element& x) noexcept
  -> task_size_type {
  CAF_ASSERT(x.mid.is_downstream_message());
  CAF_ASSERT(x.payload.match_elements<downstream_msg>());
  auto f = [](auto& content) { return task_size(content); };
  return visit(f, x.payload.get_as<downstream_msg>(0).content);
}

auto downstream_messages::id_of(mailbox_element& x) noexcept -> key_type {
  return x.content().get_as<downstream_msg>(0).slots.receiver;
}

bool downstream_messages::enabled(const nested_queue_type& q) noexcept {
  auto path = q.policy().handler.get();
  auto congested = path->mgr->congested(*path);
  CAF_LOG_DEBUG_IF(congested, "path is congested:"
                                << CAF_ARG2("slot", path->slots.receiver));
  return !congested;
}

auto downstream_messages::quantum(const nested_queue_type& q,
                                  deficit_type x) noexcept -> deficit_type {
  // TODO: adjust quantum based on the stream priority
  return x * static_cast<deficit_type>(q.policy().handler->desired_batch_size);
}

void downstream_messages::cleanup(nested_queue_type& sub_queue) noexcept {
  if (auto handler = sub_queue.policy().handler.get())
    if (auto input_buffer_size = handler->metrics.input_buffer_size)
      input_buffer_size->dec(sub_queue.total_task_size());
}

bool downstream_messages::push_back(nested_queue_type& sub_queue,
                                    mapped_type* ptr) noexcept {
  if (auto handler = sub_queue.policy().handler.get()) {
    if (auto input_buffer_size = handler->metrics.input_buffer_size)
      input_buffer_size->inc(sub_queue.policy().task_size(*ptr));
    return sub_queue.push_back(ptr);
  } else {
    unique_pointer::deleter_type d;
    d(ptr);
    return false;
  }
}

void downstream_messages::lifo_append(nested_queue_type& sub_queue,
                                      pointer ptr) noexcept {
  auto& sub_policy = sub_queue.policy();
  if (sub_policy.handler) {
    sub_policy.bulk_inserted_size += sub_policy.task_size(*ptr);
    sub_queue.lifo_append(ptr);
  } else {
    unique_pointer::deleter_type d;
    d(ptr);
  }
}

void downstream_messages::stop_lifo_append(
  nested_queue_type& sub_queue) noexcept {
  auto& sub_policy = sub_queue.policy();
  if (sub_policy.bulk_inserted_size > 0) {
    CAF_ASSERT(sub_policy.handler != nullptr);
    if (auto input_buffer_size = sub_policy.handler->metrics.input_buffer_size)
      input_buffer_size->inc(sub_policy.bulk_inserted_size);
    sub_policy.bulk_inserted_size = 0;
    sub_queue.stop_lifo_append();
  }
}

} // namespace caf::policy
