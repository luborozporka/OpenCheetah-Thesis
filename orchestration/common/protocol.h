#ifndef ORCHESTRATION_COMMON_PROTOCOL_H_
#define ORCHESTRATION_COMMON_PROTOCOL_H_

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace orchestration {

enum class Backend {
  kUnknown = 0,
  kCheetah,
  kSciHe,
};

enum class Network {
  kUnknown = 0,
  kSqnet,
  kResnet50,
  kDensenet121,
};

enum class Policy {
  kUnknown = 0,
  kRoundRobin,
  kLeastConnections,
  kEnergyAware,
};

enum class NodeRole {
  kUnknown = 0,
  kServer,
  kClient,
};

enum class RoutingStatus {
  kUnknown = 0,
  kOk,
  kNoCapacity,
  kInvalidRequest,
  kServerError,
};

struct NodeHeartbeat {
  std::string node_id;
  NodeRole node_role = NodeRole::kUnknown;
  std::string server_ip;
  Backend backend = Backend::kUnknown;
  uint16_t control_port = 0;
  uint16_t status_port = 0;
  std::vector<Network> supported_networks;
  int active_sessions = 0;
  int max_active_sessions = 0;
  uint64_t completed_sessions = 0;
  uint64_t failed_sessions = 0;
  bool power_available = false;
  double power_w = 0.0;
  double idle_power_w = 0.0;
  double dynamic_power_w = 0.0;
  uint64_t mem_available_bytes = 0;
  bool healthy = false;
  int64_t timestamp_ms = 0;
};

struct RoutingRequest {
  std::string request_id;
  Backend backend = Backend::kUnknown;
  Network network = Network::kUnknown;
  std::string input_shape;
  int32_t bitlength = 0;
  int32_t scale = 0;
  int32_t num_threads = 0;
  int64_t max_latency_ms = -1;
  double min_accuracy = -1.0;
};

struct RoutingResponse {
  RoutingStatus status = RoutingStatus::kUnknown;
  std::string request_id;
  std::string node_id;
  std::string server_ip;
  uint16_t data_port = 0;
  Backend backend = Backend::kUnknown;
  Network network = Network::kUnknown;
  std::string reason;
};

using KeyValueMessage = std::map<std::string, std::string>;

std::string ToString(Backend backend);
std::string ToString(Network network);
std::string ToString(Policy policy);
std::string ToString(NodeRole role);
std::string ToString(RoutingStatus status);

bool ParseBackend(const std::string &value, Backend *backend);
bool ParseNetwork(const std::string &value, Network *network);
bool ParsePolicy(const std::string &value, Policy *policy);
bool ParseNodeRole(const std::string &value, NodeRole *role);
bool ParseRoutingStatus(const std::string &value, RoutingStatus *status);

KeyValueMessage ParseKeyValueMessage(const std::string &message);
std::string SerializeKeyValueMessage(const KeyValueMessage &fields);

std::string SerializeHeartbeat(const NodeHeartbeat &heartbeat);
bool ParseHeartbeat(const std::string &message, NodeHeartbeat *heartbeat, std::string *error);

std::string SerializeRoutingRequest(const RoutingRequest &request);
bool ParseRoutingRequest(const std::string &message, RoutingRequest *request, std::string *error);

std::string SerializeRoutingResponse(const RoutingResponse &response);
bool ParseRoutingResponse(const std::string &message, RoutingResponse *response, std::string *error);

std::string JoinNetworks(const std::vector<Network> &networks);
std::vector<Network> SplitNetworks(const std::string &value);

}  // namespace orchestration

#endif  // ORCHESTRATION_COMMON_PROTOCOL_H_
