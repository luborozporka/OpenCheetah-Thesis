#include "orchestration/node_registry.h"

#include <mutex>

namespace orchestration {

bool NodeRegistry::RecordHeartbeat(const NodeHeartbeat &heartbeat, int64_t received_at_ms) {
  std::unique_lock<std::shared_mutex> lock(mutex_);
  const bool inserted = nodes_.find(heartbeat.node_id) == nodes_.end();
  nodes_[heartbeat.node_id] = Entry{heartbeat, received_at_ms};
  return inserted;
}

std::vector<NodeHeartbeat> NodeRegistry::Snapshot(int64_t now_ms, int64_t heartbeat_timeout_ms) const {
  std::shared_lock<std::shared_mutex> lock(mutex_);

  std::vector<NodeHeartbeat> snapshot;
  snapshot.reserve(nodes_.size());
  for (const auto &node : nodes_) {
    NodeHeartbeat heartbeat = node.second.heartbeat;
    if (heartbeat_timeout_ms >= 0 && now_ms - node.second.received_at_ms > heartbeat_timeout_ms) {
      heartbeat.healthy = false;
    }
    snapshot.push_back(heartbeat);
  }
  return snapshot;
}

size_t NodeRegistry::size() const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  return nodes_.size();
}

}
