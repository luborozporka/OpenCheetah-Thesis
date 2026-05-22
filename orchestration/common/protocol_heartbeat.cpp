#include "orchestration/common/protocol.h"

#include "orchestration/common/protocol_internal.h"

namespace orchestration {
namespace {

using protocol_internal::AddIfNotEmpty;
using protocol_internal::GetField;
using protocol_internal::HasField;
using protocol_internal::ParseBool;
using protocol_internal::ParseDouble;
using protocol_internal::ParseInt64;
using protocol_internal::ParseUint64;
using protocol_internal::RequireField;

}

std::string SerializeHeartbeat(const NodeHeartbeat &heartbeat) {
  KeyValueMessage fields;
  fields["type"] = "heartbeat";
  fields["node_id"] = heartbeat.node_id;
  fields["node_role"] = ToString(heartbeat.node_role);
  AddIfNotEmpty(&fields, "server_ip", heartbeat.server_ip);
  if (heartbeat.backend != Backend::kUnknown) {
    fields["backend"] = ToString(heartbeat.backend);
  }
  if (heartbeat.control_port != 0) {
    fields["control_port"] = std::to_string(heartbeat.control_port);
  }
  if (heartbeat.status_port != 0) {
    fields["status_port"] = std::to_string(heartbeat.status_port);
  }
  if (!heartbeat.supported_networks.empty()) {
    fields["supports"] = JoinNetworks(heartbeat.supported_networks);
  }
  fields["active_sessions"] = std::to_string(heartbeat.active_sessions);
  fields["max_active_sessions"] = std::to_string(heartbeat.max_active_sessions);
  fields["completed_sessions"] = std::to_string(heartbeat.completed_sessions);
  fields["failed_sessions"] = std::to_string(heartbeat.failed_sessions);
  fields["power_available"] = heartbeat.power_available ? "1" : "0";
  fields["power_w"] = std::to_string(heartbeat.power_w);
  fields["idle_power_w"] = std::to_string(heartbeat.idle_power_w);
  fields["dynamic_power_w"] = std::to_string(heartbeat.dynamic_power_w);
  fields["mem_available_bytes"] = std::to_string(heartbeat.mem_available_bytes);
  fields["healthy"] = heartbeat.healthy ? "1" : "0";
  fields["timestamp_ms"] = std::to_string(heartbeat.timestamp_ms);
  return SerializeKeyValueMessage(fields);
}

bool ParseHeartbeat(const std::string &message, NodeHeartbeat *heartbeat,
                    std::string *error) {
  const KeyValueMessage fields = ParseKeyValueMessage(message);
  if (!RequireField(fields, "type", error) ||
      !RequireField(fields, "node_id", error) ||
      !RequireField(fields, "node_role", error)) {
    return false;
  }
  if (GetField(fields, "type") != "heartbeat") {
    if (error != nullptr) *error = "unexpected message type";
    return false;
  }

  NodeHeartbeat parsed;
  parsed.node_id = GetField(fields, "node_id");
  if (!ParseNodeRole(GetField(fields, "node_role"), &parsed.node_role)) {
    if (error != nullptr) *error = "invalid node_role";
    return false;
  }
  parsed.server_ip = GetField(fields, "server_ip");
  if (HasField(fields, "backend")) {
    ParseBackend(GetField(fields, "backend"), &parsed.backend);
  }
  if (HasField(fields, "supports")) {
    parsed.supported_networks = SplitNetworks(GetField(fields, "supports"));
  }

  int64_t signed_value = 0;
  uint64_t unsigned_value = 0;
  double double_value = 0.0;
  bool bool_value = false;

  if (HasField(fields, "control_port") &&
      ParseInt64(GetField(fields, "control_port"), &signed_value)) {
    parsed.control_port = static_cast<uint16_t>(signed_value);
  }
  if (HasField(fields, "status_port") &&
      ParseInt64(GetField(fields, "status_port"), &signed_value)) {
    parsed.status_port = static_cast<uint16_t>(signed_value);
  }
  if (HasField(fields, "active_sessions") &&
      ParseInt64(GetField(fields, "active_sessions"), &signed_value)) {
    parsed.active_sessions = static_cast<int>(signed_value);
  }
  if (HasField(fields, "max_active_sessions") &&
      ParseInt64(GetField(fields, "max_active_sessions"), &signed_value)) {
    parsed.max_active_sessions = static_cast<int>(signed_value);
  }
  if (HasField(fields, "completed_sessions") &&
      ParseUint64(GetField(fields, "completed_sessions"), &unsigned_value)) {
    parsed.completed_sessions = unsigned_value;
  }
  if (HasField(fields, "failed_sessions") &&
      ParseUint64(GetField(fields, "failed_sessions"), &unsigned_value)) {
    parsed.failed_sessions = unsigned_value;
  }
  if (HasField(fields, "power_available") &&
      ParseBool(GetField(fields, "power_available"), &bool_value)) {
    parsed.power_available = bool_value;
  }
  if (HasField(fields, "power_w") &&
      ParseDouble(GetField(fields, "power_w"), &double_value)) {
    parsed.power_w = double_value;
  }
  if (HasField(fields, "idle_power_w") &&
      ParseDouble(GetField(fields, "idle_power_w"), &double_value)) {
    parsed.idle_power_w = double_value;
  }
  if (HasField(fields, "dynamic_power_w") &&
      ParseDouble(GetField(fields, "dynamic_power_w"), &double_value)) {
    parsed.dynamic_power_w = double_value;
  }
  if (HasField(fields, "mem_available_bytes") &&
      ParseUint64(GetField(fields, "mem_available_bytes"), &unsigned_value)) {
    parsed.mem_available_bytes = unsigned_value;
  }
  if (HasField(fields, "healthy") &&
      ParseBool(GetField(fields, "healthy"), &bool_value)) {
    parsed.healthy = bool_value;
  }
  if (HasField(fields, "timestamp_ms") &&
      ParseInt64(GetField(fields, "timestamp_ms"), &signed_value)) {
    parsed.timestamp_ms = signed_value;
  }

  *heartbeat = parsed;
  return true;
}

}  // namespace orchestration
