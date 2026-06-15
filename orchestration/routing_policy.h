#ifndef ORCHESTRATION_ROUTING_POLICY_H_
#define ORCHESTRATION_ROUTING_POLICY_H_

#include "orchestration/common/protocol.h"
#include "orchestration/knowledge_base.h"

#include <cstddef>
#include <cstdint>
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
  std::string candidate_scores;
  std::string reason;
};

std::vector<NodeHeartbeat> FilterRoutingCandidates(const std::vector<NodeHeartbeat> &nodes,
                                                   const RoutingRequest &request,
                                                   uint64_t min_mem_available_bytes,
                                                   const KnowledgeBase *knowledge_base);

RoutingDecision SelectRoutingCandidate(const std::vector<NodeHeartbeat> &nodes,
                                       const RoutingRequest &request,
                                       Policy policy,
                                       uint64_t min_mem_available_bytes,
                                       const KnowledgeBase *knowledge_base,
                                       RoutingPolicyState *state);

}

#endif