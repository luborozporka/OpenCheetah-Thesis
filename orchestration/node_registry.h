#ifndef ORCHESTRATION_NODE_REGISTRY_H_
#define ORCHESTRATION_NODE_REGISTRY_H_

#include "orchestration/common/protocol.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <shared_mutex>
#include <string>
#include <vector>

namespace orchestration {

class NodeRegistry {
 public:
  bool RecordHeartbeat(const NodeHeartbeat &heartbeat, int64_t received_at_ms);
  std::vector<NodeHeartbeat> Snapshot(int64_t now_ms, int64_t heartbeat_timeout_ms) const;
  size_t size() const;

 private:
  struct Entry {
    NodeHeartbeat heartbeat;
    int64_t received_at_ms = 0;
  };

  mutable std::shared_mutex mutex_;
  std::map<std::string, Entry> nodes_;
};

}

#endif