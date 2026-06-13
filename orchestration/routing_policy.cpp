#include "orchestration/routing_policy.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace orchestration {
namespace {

constexpr double kDefaultPredictedLatencyS = 1.0;
constexpr double kMinimumEffectivePowerW = 1.0;
constexpr double kCapacityPenaltyWeight = 1.0;

struct EnergyAwareScore {
  NodeHeartbeat candidate;
  double score = std::numeric_limits<double>::max();
  std::string source;
};

struct EnergyAwareSelection {
  NodeHeartbeat selected;
  std::string candidate_scores;
};

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

double CapacityMultiplier(const NodeHeartbeat &node) {
  const double ratio = CapacityRatio(node);
  return 1.0 + kCapacityPenaltyWeight * ratio * ratio;
}

double EffectivePowerW(const NodeHeartbeat &node) {
  return std::max(node.dynamic_power_w, kMinimumEffectivePowerW);
}

KnowledgeBaseQuery BuildKnowledgeBaseQuery(
    const NodeHeartbeat &node,
    const RoutingRequest &request) {
  KnowledgeBaseQuery query;
  query.node_id = node.node_id;
  query.backend = request.backend;
  query.network = request.network;
  query.input_shape = request.input_shape;
  query.num_threads = request.num_threads;
  query.concurrency_level = node.active_sessions + 1;
  return query;
}

bool PassesLatencyConstraint(
    const NodeHeartbeat &node,
    const RoutingRequest &request,
    const KnowledgeBase *knowledge_base) {
  if (request.max_latency_ms < 0 || knowledge_base == nullptr) return true;

  const KnowledgeBaseLookup lookup = knowledge_base->Lookup(BuildKnowledgeBaseQuery(node, request));
  if (!lookup.found || lookup.entry.mean_latency_ms <= 0.0) return true;

  return lookup.entry.mean_latency_ms <= static_cast<double>(request.max_latency_ms);
}

EnergyAwareScore ScoreEnergyAwareCandidate(
    const NodeHeartbeat &node,
    const RoutingRequest &request,
    const KnowledgeBase *knowledge_base,
    bool time_proxy_mode) {
  EnergyAwareScore result;
  result.candidate = node;
  const double capacity_multiplier = CapacityMultiplier(node);

  if (knowledge_base != nullptr) {
    const KnowledgeBaseLookup lookup = knowledge_base->Lookup(BuildKnowledgeBaseQuery(node, request));
    if (lookup.found) {
      const std::string source = ToString(lookup.source);
      if (lookup.entry.mean_energy_j > 0.0) {
        result.score = lookup.entry.mean_energy_j * capacity_multiplier;
        result.source = source;
        return result;
      }
      if (lookup.entry.mean_latency_ms > 0.0) {
        const double latency_s = lookup.entry.mean_latency_ms / 1000.0;
        if (node.power_available) {
          result.score = latency_s * EffectivePowerW(node) * capacity_multiplier;
          result.source = source + "_latency_live_power";
        } else {
          result.score = latency_s * capacity_multiplier;
          result.source = source + "_time_proxy";
        }
        return result;
      }
    }
  }

  if (time_proxy_mode) {
    result.score = kDefaultPredictedLatencyS * capacity_multiplier;
    result.source = knowledge_base == nullptr
      ? "no_kb_time_proxy"
      : "default_time_proxy";
    return result;
  }
  if (!node.power_available) {
    result.source = knowledge_base == nullptr
      ? "no_kb_no_power"
      : "default_no_power";
    return result;
  }

  result.score = kDefaultPredictedLatencyS * EffectivePowerW(node) * capacity_multiplier;
  result.source = knowledge_base == nullptr
    ? "no_kb_live_power"
    : "default_live_power";
  return result;
}

bool LowerEnergyScore(const EnergyAwareScore &lhs, const EnergyAwareScore &rhs) {
  if (lhs.score != rhs.score) return lhs.score < rhs.score;
  if (lhs.candidate.active_sessions != rhs.candidate.active_sessions) {
    return lhs.candidate.active_sessions < rhs.candidate.active_sessions;
  }
  return lhs.candidate.node_id < rhs.candidate.node_id;
}

std::string ScoreValueString(double score) {
  if (score == std::numeric_limits<double>::max()) return "inf";
  return std::to_string(score);
}

std::string JoinCandidateScores(const std::vector<EnergyAwareScore> &scores) {
  std::string result;
  for (const auto &score : scores) {
    if (!result.empty()) result += ';';
    result += score.candidate.node_id + "=" + ScoreValueString(score.score) + ":" + score.source;
  }
  return result;
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

EnergyAwareSelection SelectEnergyAwareCandidate(
    const std::vector<NodeHeartbeat> &candidates,
    const RoutingRequest &request,
    const KnowledgeBase *knowledge_base) {
  const bool time_proxy_mode = std::none_of(
      candidates.begin(),
      candidates.end(),
      [](const auto &candidate) {
        return candidate.power_available;
      });

  std::vector<EnergyAwareScore> scores;
  scores.reserve(candidates.size());
  for (const auto &candidate : candidates) {
    scores.push_back(ScoreEnergyAwareCandidate(candidate, request, knowledge_base, time_proxy_mode));
  }

  const auto selected = std::min_element(scores.begin(), scores.end(), LowerEnergyScore);

  EnergyAwareSelection selection;
  selection.selected = selected->candidate;
  selection.candidate_scores = JoinCandidateScores(scores);
  return selection;
}

}  // namespace

std::vector<NodeHeartbeat> FilterRoutingCandidates(
    const std::vector<NodeHeartbeat> &nodes,
    const RoutingRequest &request,
    uint64_t min_mem_available_bytes,
    const KnowledgeBase *knowledge_base) {
  std::vector<NodeHeartbeat> candidates;
  for (const auto &node : nodes) {
    if (node.node_role != NodeRole::kServer) continue;
    if (!node.healthy) continue;
    if (node.backend != request.backend) continue;
    if (!SupportsNetwork(node, request.network)) continue;
    if (node.server_ip.empty() || node.control_port == 0) continue;
    if (node.max_active_sessions <= 0) continue;
    if (node.active_sessions >= node.max_active_sessions) continue;
    if (min_mem_available_bytes > 0 && node.mem_available_bytes < min_mem_available_bytes) {
      continue;
    }
    if (!PassesLatencyConstraint(node, request, knowledge_base)) continue;
    candidates.push_back(node);
  }
  return candidates;
}

RoutingDecision SelectRoutingCandidate(
    const std::vector<NodeHeartbeat> &nodes,
    const RoutingRequest &request,
    Policy policy,
    uint64_t min_mem_available_bytes,
    const KnowledgeBase *knowledge_base,
    RoutingPolicyState *state) {
  RoutingDecision decision;
  decision.candidates = FilterRoutingCandidates(nodes, request, min_mem_available_bytes, knowledge_base);
  if (decision.candidates.empty()) {
    decision.reason = "no compatible healthy node below capacity, memory, and latency constraints";
    return decision;
  }

  switch (policy) {
    case Policy::kRoundRobin:
      decision.selected = SelectRoundRobinCandidate(decision.candidates, request, state);
      break;
    case Policy::kLeastConnections:
      decision.selected = SelectLeastConnectionsCandidate(decision.candidates);
      break;
    case Policy::kEnergyAware: {
      const EnergyAwareSelection selection = SelectEnergyAwareCandidate(decision.candidates, request, knowledge_base);
      decision.selected = selection.selected;
      decision.candidate_scores = selection.candidate_scores;
      break;
    }
    case Policy::kUnknown:
      decision.reason = "unsupported routing policy";
      return decision;
  }

  decision.ok = true;
  return decision;
}

}  // namespace orchestration
