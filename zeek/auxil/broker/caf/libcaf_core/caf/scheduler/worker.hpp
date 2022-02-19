// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstddef>

#include "caf/detail/double_ended_queue.hpp"
#include "caf/detail/set_thread_name.hpp"
#include "caf/execution_unit.hpp"
#include "caf/logger.hpp"
#include "caf/resumable.hpp"

namespace caf::scheduler {

template <class Policy>
class coordinator;

/// Policy-based implementation of the abstract worker base class.
template <class Policy>
class worker : public execution_unit {
public:
  using job_ptr = resumable*;
  using coordinator_ptr = coordinator<Policy>*;
  using policy_data = typename Policy::worker_data;

  worker(size_t worker_id, coordinator_ptr worker_parent,
         const policy_data& init, size_t throughput)
    : execution_unit(&worker_parent->system()),
      max_throughput_(throughput),
      id_(worker_id),
      parent_(worker_parent),
      data_(init) {
    // nop
  }

  void start() {
    CAF_ASSERT(this_thread_.get_id() == std::thread::id{});
    this_thread_ = system().launch_thread("caf.worker", [this] { run(); });
  }

  worker(const worker&) = delete;
  worker& operator=(const worker&) = delete;

  /// Enqueues a new job to the worker's queue from an external
  /// source, i.e., from any other thread.
  void external_enqueue(job_ptr job) {
    CAF_ASSERT(job != nullptr);
    policy_.external_enqueue(this, job);
  }

  /// Enqueues a new job to the worker's queue from an internal
  /// source, i.e., a job that is currently executed by this worker.
  /// @warning Must not be called from other threads.
  void exec_later(job_ptr job) override {
    CAF_ASSERT(job != nullptr);
    policy_.internal_enqueue(this, job);
  }

  coordinator_ptr parent() {
    return parent_;
  }

  size_t id() const {
    return id_;
  }

  std::thread& get_thread() {
    return this_thread_;
  }

  actor_id id_of(resumable* ptr) {
    abstract_actor* dptr = ptr != nullptr ? dynamic_cast<abstract_actor*>(ptr)
                                          : nullptr;
    return dptr != nullptr ? dptr->id() : 0;
  }

  policy_data& data() {
    return data_;
  }

  size_t max_throughput() {
    return max_throughput_;
  }

private:
  void run() {
    CAF_SET_LOGGER_SYS(&system());
    // scheduling loop
    for (;;) {
      auto job = policy_.dequeue(this);
      CAF_ASSERT(job != nullptr);
      CAF_ASSERT(job->subtype() != resumable::io_actor);
      policy_.before_resume(this, job);
      auto res = job->resume(this, max_throughput_);
      policy_.after_resume(this, job);
      switch (res) {
        case resumable::resume_later: {
          // keep reference to this actor, as it remains in the "loop"
          policy_.resume_job_later(this, job);
          break;
        }
        case resumable::done: {
          policy_.after_completion(this, job);
          intrusive_ptr_release(job);
          break;
        }
        case resumable::awaiting_message: {
          // resumable will maybe be enqueued again later, deref it for now
          intrusive_ptr_release(job);
          break;
        }
        case resumable::shutdown_execution_unit: {
          policy_.after_completion(this, job);
          policy_.before_shutdown(this);
          return;
        }
      }
    }
  }
  // number of messages each actor is allowed to consume per resume
  size_t max_throughput_;
  // the worker's thread
  std::thread this_thread_;
  // the worker's ID received from scheduler
  size_t id_;
  // pointer to central coordinator
  coordinator_ptr parent_;
  // policy-specific data
  policy_data data_;
  // instance of our policy object
  Policy policy_;
};

} // namespace caf::scheduler
