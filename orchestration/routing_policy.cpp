#include "orchestration/routing_policy.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace orchestration {
namespace {

constexpr double kDefaultPredictedLatencyS = 1.0;
constexpr double kMinimumEffectivePowerW = 1.0;
constexpr double kCapacityPenaltyWeight = 1.0;

bool SupportsNetwork(const NodeHeartbeat &node, Network network) {
  for (const Network supported : node.supported_networks) {
    if (supported == network) return true;
  }
  return false;
}

std::string RoutingKey(const RoutingRequest &request) {
  return ToString(request.backend) + "|" + ToString(request.network);
}

double CapacityRatio(const NodeHeartbeat &node) {
  if (node.max_active_sessions <= 0) return 1.0;
  return static_cast<double>(node.active_sessions) /
         static_cast<double>(node.max_active_sessions);
}

double CapacityPenalty(const NodeHeartbeat &node) {
  const double ratio = CapacityRatio(node);
  return kCapacityPenaltyWeight * ratio * ratio;
}

NodeHeartbeat SelectRoundRobinCandidate(
    std::vector<NodeHeartbeat> candidates,
    const RoutingRequest &request,
    RoutingPolicyState *state) {
  std::sort(
      candidates.begin(),
      candidates.end(),
      [](const auto &lhs, const auto &rhs) {
        return lhs.node_id < rhs.node_id;
      });

  std::lock_guard<std::mutex> lock(state->mutex);
  size_t &next = state->round_robin_next[RoutingKey(request)];
  const size_t selected_index = next % candidates.size();
  next = (selected_index + 1) % candidates.size();
  return candidates[selected_index];
}

NodeHeartbeat SelectLeastConnectionsCandidate(
    std::vector<NodeHeartbeat> candidates) {
  return *std::min_element(
      candidates.begin(),
      candidates.end(),
      [](const auto &lhs, const auto &rhs) {
        if (lhs.active_sessions != rhs.active_sessions) {
          return lhs.active_sessions < rhs.active_sessions;
        }
        if (lhs.dynamic_power_w != rhs.dynamic_power_w) {
          return lhs.dynamic_power_w < rhs.dynamic_power_w;
        }
        return lhs.node_id < rhs.node_id;
      });
}

NodeHeartbeat SelectEnergyAwareCandidate(
    std::vector<NodeHeartbeat> candidates) {
  const bool time_proxy_mode = std::none_of(
      candidates.begin(),
      candidates.end(),
      [](const auto &candidate) {
        return candidate.power_available;
      });

  return *std::min_element(
      candidates.begin(),
      candidates.end(),
      [time_proxy_mode](const auto &lhs, const auto &rhs) {
        const auto score = [time_proxy_mode](const auto &node) {
          if (time_proxy_mode) {
            return kDefaultPredictedLatencyS + CapacityPenalty(node);
          }
          if (!node.power_available) {
            return std::numeric_limits<double>::max();
          }
          const double effective_power_w =
              std::max(node.dynamic_power_w, kMinimumEffectivePowerW);
          return kDefaultPredictedLatencyS * effective_power_w +
                 CapacityPenalty(node);
        };

        const double lhs_score = score(lhs);
        const double rhs_score = score(rhs);
        if (lhs_score != rhs_score) return lhs_score < rhs_score;
        if (lhs.active_sessions != rhs.active_sessions) {
          return lhs.active_sessions < rhs.active_sessions;
        }
        return lhs.node_id < rhs.node_id;
      });
}

}  // namespace

std::vector<NodeHeartbeat> FilterRoutingCandidates(
    const std::vector<NodeHeartbeat> &nodes,
    const RoutingRequest &request) {
  std::vector<NodeHeartbeat> candidates;
  for (const auto &node : nodes) {
    if (node.node_role != NodeRole::kServer) continue;
    if (!node.healthy) continue;
    if (node.backend != request.backend) continue;
    if (!SupportsNetwork(node, request.network)) continue;
    if (node.server_ip.empty() || node.control_port == 0) continue;
    if (node.max_active_sessions <= 0) continue;
    if (node.active_sessions >= node.max_active_sessions) continue;
    candidates.push_back(node);
  }
  return candidates;
}

RoutingDecision SelectRoutingCandidate(
    const std::vector<NodeHeartbeat> &nodes,
    const RoutingRequest &request,
    Policy policy,
    RoutingPolicyState *state) {
  RoutingDecision decision;
  decision.candidates = FilterRoutingCandidates(nodes, request);
  if (decision.candidates.empty()) {
    decision.reason = "no compatible healthy node below capacity";
    return decision;
  }

  switch (policy) {
    case Policy::kRoundRobin:
      decision.selected =
          SelectRoundRobinCandidate(decision.candidates, request, state);
      break;
    case Policy::kLeastConnections:
      decision.selected = SelectLeastConnectionsCandidate(decision.candidates);
      break;
    case Policy::kEnergyAware:
      decision.selected = SelectEnergyAwareCandidate(decision.candidates);
      break;
    case Policy::kUnknown:
      decision.reason = "unsupported routing policy";
      return decision;
  }

  decision.ok = true;
  return decision;
}

}  // namespace orchestration
