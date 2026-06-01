#include "orchestration/common/protocol.h"

#include "orchestration/common/protocol_internal.h"

namespace orchestration {
namespace {

using protocol_internal::AddIfNotEmpty;
using protocol_internal::GetField;
using protocol_internal::HasField;
using protocol_internal::ParseDouble;
using protocol_internal::ParseInt64;
using protocol_internal::RequireField;

}  // namespace

std::string SerializeRoutingRequest(const RoutingRequest &request) {
  KeyValueMessage fields;
  fields["type"] = "routing_request";
  fields["request_id"] = request.request_id;
  fields["backend"] = ToString(request.backend);
  fields["network"] = ToString(request.network);
  AddIfNotEmpty(&fields, "input_shape", request.input_shape);
  fields["bitlength"] = std::to_string(request.bitlength);
  fields["scale"] = std::to_string(request.scale);
  fields["num_threads"] = std::to_string(request.num_threads);
  if (request.max_latency_ms >= 0) {
    fields["max_latency_ms"] = std::to_string(request.max_latency_ms);
  }
  if (request.min_accuracy >= 0.0) {
    fields["min_accuracy"] = std::to_string(request.min_accuracy);
  }
  return SerializeKeyValueMessage(fields);
}

bool ParseRoutingRequest(const std::string &message, RoutingRequest *request,
                         std::string *error) {
  const KeyValueMessage fields = ParseKeyValueMessage(message);
  if (!RequireField(fields, "type", error) ||
      !RequireField(fields, "request_id", error) ||
      !RequireField(fields, "backend", error) ||
      !RequireField(fields, "network", error)) {
    return false;
  }
  if (GetField(fields, "type") != "routing_request") {
    if (error != nullptr) *error = "unexpected message type";
    return false;
  }
  if (HasField(fields, "policy")) {
    if (error != nullptr) *error = "policy is configured by orchestrator";
    return false;
  }

  RoutingRequest parsed;
  parsed.request_id = GetField(fields, "request_id");
  parsed.input_shape = GetField(fields, "input_shape");
  if (!ParseBackend(GetField(fields, "backend"), &parsed.backend)) {
    if (error != nullptr) *error = "invalid backend";
    return false;
  }
  if (!ParseNetwork(GetField(fields, "network"), &parsed.network)) {
    if (error != nullptr) *error = "invalid network";
    return false;
  }

  int64_t signed_value = 0;
  double double_value = 0.0;
  if (HasField(fields, "bitlength") &&
      ParseInt64(GetField(fields, "bitlength"), &signed_value)) {
    parsed.bitlength = static_cast<int32_t>(signed_value);
  }
  if (HasField(fields, "scale") &&
      ParseInt64(GetField(fields, "scale"), &signed_value)) {
    parsed.scale = static_cast<int32_t>(signed_value);
  }
  if (HasField(fields, "num_threads") &&
      ParseInt64(GetField(fields, "num_threads"), &signed_value)) {
    parsed.num_threads = static_cast<int32_t>(signed_value);
  }
  if (HasField(fields, "max_latency_ms") &&
      ParseInt64(GetField(fields, "max_latency_ms"), &signed_value)) {
    parsed.max_latency_ms = signed_value;
  }
  if (HasField(fields, "min_accuracy") &&
      ParseDouble(GetField(fields, "min_accuracy"), &double_value)) {
    parsed.min_accuracy = double_value;
  }

  *request = parsed;
  return true;
}

std::string SerializeRoutingResponse(const RoutingResponse &response) {
  KeyValueMessage fields;
  fields["type"] = "routing_response";
  fields["status"] = ToString(response.status);
  fields["request_id"] = response.request_id;
  AddIfNotEmpty(&fields, "node_id", response.node_id);
  AddIfNotEmpty(&fields, "server_ip", response.server_ip);
  if (response.data_port != 0) {
    fields["data_port"] = std::to_string(response.data_port);
  }
  if (response.backend != Backend::kUnknown) {
    fields["backend"] = ToString(response.backend);
  }
  if (response.network != Network::kUnknown) {
    fields["network"] = ToString(response.network);
  }
  AddIfNotEmpty(&fields, "reason", response.reason);
  return SerializeKeyValueMessage(fields);
}

bool ParseRoutingResponse(const std::string &message, RoutingResponse *response,
                          std::string *error) {
  const KeyValueMessage fields = ParseKeyValueMessage(message);
  if (!RequireField(fields, "type", error) ||
      !RequireField(fields, "status", error) ||
      !RequireField(fields, "request_id", error)) {
    return false;
  }
  if (GetField(fields, "type") != "routing_response") {
    if (error != nullptr) *error = "unexpected message type";
    return false;
  }

  RoutingResponse parsed;
  parsed.request_id = GetField(fields, "request_id");
  parsed.node_id = GetField(fields, "node_id");
  parsed.server_ip = GetField(fields, "server_ip");
  parsed.reason = GetField(fields, "reason");
  if (!ParseRoutingStatus(GetField(fields, "status"), &parsed.status)) {
    if (error != nullptr) *error = "invalid status";
    return false;
  }
  if (HasField(fields, "backend")) {
    ParseBackend(GetField(fields, "backend"), &parsed.backend);
  }
  if (HasField(fields, "network")) {
    ParseNetwork(GetField(fields, "network"), &parsed.network);
  }
  int64_t signed_value = 0;
  if (HasField(fields, "data_port") &&
      ParseInt64(GetField(fields, "data_port"), &signed_value)) {
    parsed.data_port = static_cast<uint16_t>(signed_value);
  }

  *response = parsed;
  return true;
}

}  // namespace orchestration
