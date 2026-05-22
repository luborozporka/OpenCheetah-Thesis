#include "orchestration/common/protocol.h"

#include "orchestration/common/enum_parse.h"
#include "orchestration/common/protocol_internal.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <string_view>
#include <utility>

namespace orchestration::protocol_internal {

std::string Trim(const std::string &value) {
  size_t first = 0;
  while (first < value.size() && std::isspace(static_cast<unsigned char>(value[first]))) {
    ++first;
  }

  size_t last = value.size();
  while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1]))) {
    --last;
  }

  return value.substr(first, last - first);
}

std::string Lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) { return std::tolower(ch); });
  return value;
}

bool ParseBool(const std::string &value, bool *out) {
  const std::string normalized = Lower(Trim(value));
  if (normalized == "1" || normalized == "true" || normalized == "yes") {
    *out = true;
    return true;
  }
  if (normalized == "0" || normalized == "false" || normalized == "no") {
    *out = false;
    return true;
  }
  return false;
}

bool ParseInt64(const std::string &value, int64_t *out) {
  char *end = nullptr;
  const std::string trimmed = Trim(value);
  const long long parsed = std::strtoll(trimmed.c_str(), &end, 10);
  if (end == trimmed.c_str() || *end != '\0') return false;
  *out = static_cast<int64_t>(parsed);
  return true;
}

bool ParseUint64(const std::string &value, uint64_t *out) {
  char *end = nullptr;
  const std::string trimmed = Trim(value);
  const unsigned long long parsed = std::strtoull(trimmed.c_str(), &end, 10);
  if (end == trimmed.c_str() || *end != '\0') return false;
  *out = static_cast<uint64_t>(parsed);
  return true;
}

bool ParseDouble(const std::string &value, double *out) {
  char *end = nullptr;
  const std::string trimmed = Trim(value);
  const double parsed = std::strtod(trimmed.c_str(), &end);
  if (end == trimmed.c_str() || *end != '\0') return false;
  *out = parsed;
  return true;
}

bool HasField(const KeyValueMessage &fields, const std::string &key) {
  return fields.find(key) != fields.end();
}

const std::string &GetField(const KeyValueMessage &fields, const std::string &key) {
  static const std::string kEmpty;
  const auto it = fields.find(key);
  return it == fields.end() ? kEmpty : it->second;
}

bool RequireField(const KeyValueMessage &fields, const std::string &key, std::string *error) {
  if (HasField(fields, key)) return true;
  if (error != nullptr) *error = "missing field: " + key;
  return false;
}

void AddIfNotEmpty(KeyValueMessage *fields, const std::string &key, const std::string &value) {
  if (!value.empty()) (*fields)[key] = value;
}

}  // namespace orchestration::protocol_internal

namespace orchestration {

using protocol_internal::Lower;
using protocol_internal::Trim;

std::string ToString(Backend backend) {
  switch (backend) {
    case Backend::kCheetah:
      return "cheetah";
    case Backend::kSciHe:
      return "sci-he";
    case Backend::kUnknown:
      return "unknown";
  }
  return "unknown";
}

std::string ToString(Network network) {
  switch (network) {
    case Network::kSqnet:
      return "sqnet";
    case Network::kResnet50:
      return "resnet50";
    case Network::kDensenet121:
      return "densenet121";
    case Network::kUnknown:
      return "unknown";
  }
  return "unknown";
}

std::string ToString(Policy policy) {
  switch (policy) {
    case Policy::kRoundRobin:
      return "round_robin";
    case Policy::kLeastConnections:
      return "least_connections";
    case Policy::kEnergyAware:
      return "energy_aware";
    case Policy::kUnknown:
      return "unknown";
  }
  return "unknown";
}

std::string ToString(NodeRole role) {
  switch (role) {
    case NodeRole::kServer:
      return "server";
    case NodeRole::kClient:
      return "client";
    case NodeRole::kUnknown:
      return "unknown";
  }
  return "unknown";
}

std::string ToString(RoutingStatus status) {
  switch (status) {
    case RoutingStatus::kOk:
      return "ok";
    case RoutingStatus::kNoCapacity:
      return "no_capacity";
    case RoutingStatus::kInvalidRequest:
      return "invalid_request";
    case RoutingStatus::kServerError:
      return "server_error";
    case RoutingStatus::kUnknown:
      return "unknown";
  }
  return "unknown";
}

bool ParseBackend(const std::string &value, Backend *backend) {
  static constexpr std::array<std::pair<std::string_view, Backend>, 4>
      kEntries = {{{"cheetah", Backend::kCheetah},
                   {"sci-he", Backend::kSciHe},
                   {"sci_he", Backend::kSciHe},
                   {"scihe", Backend::kSciHe}}};
  return protocol_internal::ParseEnum(value, kEntries, Backend::kUnknown, backend);
}

bool ParseNetwork(const std::string &value, Network *network) {
  static constexpr std::array<std::pair<std::string_view, Network>, 4>
      kEntries = {{{"sqnet", Network::kSqnet},
                   {"squeezenet", Network::kSqnet},
                   {"resnet50", Network::kResnet50},
                   {"densenet121", Network::kDensenet121}}};
  return protocol_internal::ParseEnum(value, kEntries, Network::kUnknown, network);
}

bool ParsePolicy(const std::string &value, Policy *policy) {
  static constexpr std::array<std::pair<std::string_view, Policy>, 6>
      kEntries = {{{"round_robin", Policy::kRoundRobin},
                   {"round-robin", Policy::kRoundRobin},
                   {"least_connections", Policy::kLeastConnections},
                   {"least-connections", Policy::kLeastConnections},
                   {"energy_aware", Policy::kEnergyAware},
                   {"energy-aware", Policy::kEnergyAware}}};
  return protocol_internal::ParseEnum(value, kEntries, Policy::kUnknown, policy);
}

bool ParseNodeRole(const std::string &value, NodeRole *role) {
  static constexpr std::array<std::pair<std::string_view, NodeRole>, 2>
      kEntries = {{{"server", NodeRole::kServer},
                   {"client", NodeRole::kClient}}};
  return protocol_internal::ParseEnum(value, kEntries, NodeRole::kUnknown, role);
}

bool ParseRoutingStatus(const std::string &value, RoutingStatus *status) {
  static constexpr std::array<std::pair<std::string_view, RoutingStatus>, 4>
      kEntries = {{{"ok", RoutingStatus::kOk},
                   {"no_capacity", RoutingStatus::kNoCapacity},
                   {"invalid_request", RoutingStatus::kInvalidRequest},
                   {"server_error", RoutingStatus::kServerError}}};
  return protocol_internal::ParseEnum(value, kEntries, RoutingStatus::kUnknown, status);
}

KeyValueMessage ParseKeyValueMessage(const std::string &message) {
  KeyValueMessage fields;
  std::istringstream input(message);
  std::string line;

  while (std::getline(input, line)) {
    line = Trim(line);
    if (line.empty() || line[0] == '#') continue;

    const size_t equals = line.find('=');
    if (equals == std::string::npos) continue;

    const std::string key = Trim(line.substr(0, equals));
    const std::string value = Trim(line.substr(equals + 1));
    if (!key.empty()) fields[key] = value;
  }

  return fields;
}

std::string SerializeKeyValueMessage(const KeyValueMessage &fields) {
  std::ostringstream output;
  for (const auto &field : fields) {
    output << field.first << '=' << field.second << '\n';
  }
  return output.str();
}

std::string JoinNetworks(const std::vector<Network> &networks) {
  std::ostringstream output;
  for (size_t i = 0; i < networks.size(); ++i) {
    if (i > 0) output << ',';
    output << ToString(networks[i]);
  }
  return output.str();
}

std::vector<Network> SplitNetworks(const std::string &value) {
  std::vector<Network> networks;
  std::istringstream input(value);
  std::string item;

  while (std::getline(input, item, ',')) {
    Network network = Network::kUnknown;
    if (ParseNetwork(item, &network)) networks.push_back(network);
  }

  return networks;
}

}  // namespace orchestration
