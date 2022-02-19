// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "caf/detail/io_export.hpp"
#include "caf/io/abstract_broker.hpp"
#include "caf/node_id.hpp"

namespace caf::io::basp {

/// @addtogroup BASP
/// @{

/// Stores routing information for a single broker participating as
/// BASP peer and provides both direct and indirect paths.
class CAF_IO_EXPORT routing_table {
public:
  explicit routing_table(abstract_broker* parent);

  virtual ~routing_table();

  /// Describes a routing path to a node.
  struct route {
    const node_id& next_hop;
    connection_handle hdl;
  };

  /// Returns a route to `target` or `none` on error.
  optional<route> lookup(const node_id& target);

  /// Returns the ID of the peer connected via `hdl` or
  /// `none` if `hdl` is unknown.
  node_id lookup_direct(const connection_handle& hdl) const;

  /// Returns the handle offering a direct connection to `nid` or
  /// `invalid_connection_handle` if no direct connection to `nid` exists.
  optional<connection_handle> lookup_direct(const node_id& nid) const;

  /// Returns the next hop that would be chosen for `nid`
  /// or `none` if there's no indirect route to `nid`.
  node_id lookup_indirect(const node_id& nid) const;

  /// Adds a new direct route to the table.
  /// @pre `hdl != invalid_connection_handle && nid != none`
  void add_direct(const connection_handle& hdl, const node_id& nid);

  /// Adds a new indirect route to the table.
  bool add_indirect(const node_id& hop, const node_id& dest);

  /// Removes a direct connection and return the node ID that became
  /// unreachable as a result of this operation.
  node_id erase_direct(const connection_handle& hdl);

  /// Removes any entry for indirect connection to `dest` and returns
  /// `true` if `dest` had an indirect route, otherwise `false`.
  bool erase_indirect(const node_id& dest);

  /// Returns the parent broker.
  abstract_broker* parent() {
    return parent_;
  }

public:
  using node_id_set = std::unordered_set<node_id>;

  abstract_broker* parent_;
  mutable std::mutex mtx_;
  std::unordered_map<connection_handle, node_id> direct_by_hdl_;
  std::unordered_map<node_id, connection_handle> direct_by_nid_;
  std::unordered_map<node_id, node_id_set> indirect_;
};

/// @}

} // namespace caf::io::basp
