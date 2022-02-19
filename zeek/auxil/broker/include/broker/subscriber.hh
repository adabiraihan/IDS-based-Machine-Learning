#pragma once

#include <vector>
#include <functional>

#include <caf/actor.hpp>

#include "broker/data.hh"
#include "broker/fwd.hh"
#include "broker/message.hh"
#include "broker/subscriber_base.hh"
#include "broker/topic.hh"

#include "broker/detail/shared_subscriber_queue.hh"

namespace broker {

/// Provides blocking access to a stream of data.
class subscriber : public subscriber_base<data_message> {
public:
  // --- friend declarations ---------------------------------------------------

  friend class endpoint;
  friend class status_subscriber;

  // --- nested types ----------------------------------------------------------

  using super = subscriber_base<data_message>;

  // --- constructors and destructors ------------------------------------------

  subscriber(subscriber&&) = default;

  subscriber& operator=(subscriber&&) = default;

  ~subscriber();

  // --- properties ------------------------------------------------------------

  /// Enables or disables rate calculation. On by default.
  void set_rate_calculation(bool x);

  size_t rate() const;

  const caf::actor& worker() const {
    return worker_;
  }

  // --- topic management ------------------------------------------------------

  void add_topic(topic x, bool block = false);

  void remove_topic(topic x, bool block = false);

  // --- miscellaneous ---------------------------------------------------------

  /// Release any state held by the object, rendering it invalid.
  /// @warning Performing *any* action on this object afterwards invokes
  ///          undefined behavior, except:
  ///          - Destroying the object by calling the destructor.
  ///          - Using copy- or move-assign from a valid `store` to "revive"
  ///            this object.
  ///          - Calling `reset` again (multiple invocations are no-ops).
  /// @note This member function specifically targets the Python bindings. When
  ///       writing Broker applications using the native C++ API, there's no
  ///       point in calling this member function.
  void reset();

protected:
  void became_not_full() override;

private:
  // -- force users to use `endpoint::make_status_subscriber` ------------------
  subscriber(endpoint& ep, std::vector<topic> ts, size_t max_qsize);

  caf::actor worker_;
  std::vector<topic> filter_;
  std::reference_wrapper<endpoint> ep_;
};

} // namespace broker
