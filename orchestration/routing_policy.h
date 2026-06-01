#ifndef ORCHESTRATION_ROUTING_POLICY_H_
#define ORCHESTRATION_ROUTING_POLICY_H_

#include "orchestration/common/protocol.h"

#include <cstddef>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace orchestration {

struct RoutingPolicyState {
  std::mutex mutex;
  std::map<std::string, size_t> round_robin_next;
};

struct RoutingDecision {
  bool ok = false;
  NodeHeartbeat selected;
  std::vector<NodeHeartbeat> candidates;
  std::string reason;
};

std::vector<NodeHeartbeat> FilterRoutingCandidates(
  const std::vector<NodeHeartbeat> &nodes,
  const RoutingRequest &request);

RoutingDecision SelectRoutingCandidate(
  const std::vector<NodeHeartbeat> &nodes,
  const RoutingRequest &request,
  Policy policy,
  RoutingPolicyState *state);

}

#endif
