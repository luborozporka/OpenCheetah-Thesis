#include <asio.hpp>

#include "orchestration/common/protocol.h"
#include "orchestration/common/time_utils.h"
#include "orchestration/power_sensor.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

struct Config {
  std::string node_id;
  orchestration::NodeRole node_role = orchestration::NodeRole::kUnknown;
  std::string server_ip;
  orchestration::Backend backend = orchestration::Backend::kUnknown;
  uint16_t control_port = 0;
  uint16_t status_port = 0;
  double idle_power_w = 0.0;
  std::string power_path;
  bool allow_no_power_sensor = false;
};

std::map<std::string, std::string> ParseArgs(int argc, char **argv) {
  std::map<std::string, std::string> args;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    const size_t equals = arg.find('=');
    if (equals == std::string::npos || equals == 0) {
      throw std::runtime_error("Arguments must use name=value format");
    }
    args[arg.substr(0, equals)] = arg.substr(equals + 1);
  }
  return args;
}

std::string GetArg(const std::map<std::string, std::string> &args, const std::string &key, const std::string &default_value = "") {
  const auto it = args.find(key);
  return it == args.end() ? default_value : it->second;
}

uint16_t ParsePort(const std::string &value, const std::string &field_name) {
  if (value.empty()) return 0;
  const int port = std::stoi(value);
  if (port <= 0 || port > 65535) {
    throw std::runtime_error(field_name + " must be in range 1..65535");
  }
  return static_cast<uint16_t>(port);
}

double ParseDoubleArg(const std::string &value, const std::string &field_name) {
  if (value.empty()) return 0.0;
  size_t parsed = 0;
  const double result = std::stod(value, &parsed);
  if (parsed != value.size()) {
    throw std::runtime_error(field_name + " must be a number");
  }
  return result;
}

bool ParseBoolArg(const std::string &value, const std::string &field_name) {
  if (value.empty()) return false;
  if (value == "1" || value == "true" || value == "yes") return true;
  if (value == "0" || value == "false" || value == "no") return false;
  throw std::runtime_error(field_name + " must be 0/1, true/false, or yes/no");
}

Config ParseConfig(int argc, char **argv) {
  const auto args = ParseArgs(argc, argv);

  Config config;
  config.node_id = GetArg(args, "node_id");
  if (!orchestration::ParseNodeRole(GetArg(args, "node_role"), &config.node_role)) {
    throw std::runtime_error("node_role must be server or client");
  }
  if (GetArg(args, "backend") != "" &&
      !orchestration::ParseBackend(GetArg(args, "backend"), &config.backend)) {
    throw std::runtime_error("backend must be cheetah or sci-he");
  }
  config.server_ip = GetArg(args, "server_ip");
  config.control_port = ParsePort(GetArg(args, "control_port"), "control_port");
  config.status_port = ParsePort(GetArg(args, "status_port"), "status_port");
  config.idle_power_w = ParseDoubleArg(GetArg(args, "idle_power_w"), "idle_power_w");
  config.power_path = GetArg(args, "power_path");
  config.allow_no_power_sensor = ParseBoolArg(GetArg(args, "allow_no_power_sensor"), "allow_no_power_sensor");

  if (config.node_id.empty()) {
    throw std::runtime_error("node_id is required");
  }
  if (config.node_role == orchestration::NodeRole::kServer) {
    if (config.server_ip.empty()) throw std::runtime_error("server_ip is required for server nodes");
    if (config.backend == orchestration::Backend::kUnknown) {
      throw std::runtime_error("backend is required for server nodes");
    }
    if (config.status_port == 0) throw std::runtime_error("status_port is required for server nodes");
  }

  return config;
}

std::string ReadTcpResponse(const std::string &host, uint16_t port) {
  asio::io_context io;
  asio::ip::tcp::resolver resolver(io);
  asio::ip::tcp::socket socket(io);
  asio::connect(socket, resolver.resolve(host, std::to_string(port)));

  std::string response;
  char buffer[4096];
  asio::error_code ec;
  while (true) {
    const size_t bytes = socket.read_some(asio::buffer(buffer), ec);
    if (bytes > 0) response.append(buffer, bytes);
    if (ec == asio::error::eof) break;
    if (ec) throw asio::system_error(ec);
  }
  return response;
}

bool ParseIntField(const orchestration::KeyValueMessage &fields, const std::string &key, int *out) {
  const auto it = fields.find(key);
  if (it == fields.end()) return false;
  *out = std::stoi(it->second);
  return true;
}

bool ParseUint64Field(const orchestration::KeyValueMessage &fields, const std::string &key, uint64_t *out) {
  const auto it = fields.find(key);
  if (it == fields.end()) return false;
  *out = static_cast<uint64_t>(std::stoull(it->second));
  return true;
}

bool ParseBoolField(const orchestration::KeyValueMessage &fields, const std::string &key, bool *out) {
  const auto it = fields.find(key);
  if (it == fields.end()) return false;
  *out = it->second == "1" || it->second == "true" || it->second == "yes";
  return true;
}

uint64_t ReadMemAvailableBytes() {
  std::ifstream input("/proc/meminfo");
  std::string key;
  uint64_t value_kib = 0;
  std::string unit;

  while (input >> key >> value_kib >> unit) {
    if (key == "MemAvailable:") return value_kib * 1024;
  }
  return 0;
}

orchestration::NodeHeartbeat MakeBaseHeartbeat(const Config &config) {
  orchestration::NodeHeartbeat heartbeat;
  heartbeat.node_id = config.node_id;
  heartbeat.node_role = config.node_role;
  heartbeat.server_ip = config.server_ip;
  heartbeat.backend = config.backend;
  heartbeat.control_port = config.control_port;
  heartbeat.status_port = config.status_port;
  heartbeat.idle_power_w = config.idle_power_w;
  heartbeat.mem_available_bytes = ReadMemAvailableBytes();
  heartbeat.healthy = config.node_role == orchestration::NodeRole::kClient;
  heartbeat.timestamp_ms = orchestration::NowMillis();
  return heartbeat;
}

void ReadPower(const Config &config,
               orchestration::PowerSensor *sensor,
               orchestration::NodeHeartbeat *heartbeat) {
  const orchestration::PowerReading reading = sensor->Read();
  heartbeat->power_available = reading.available;
  heartbeat->power_w = reading.power_w;
  heartbeat->dynamic_power_w = reading.available
    ? std::max(0.0, reading.power_w - config.idle_power_w)
    : 0.0;
}

void PollServerStatus(const Config &config, orchestration::NodeHeartbeat *heartbeat) {
  const std::string body = ReadTcpResponse(config.server_ip, config.status_port);
  const orchestration::KeyValueMessage fields = orchestration::ParseKeyValueMessage(body);

  ParseIntField(fields, "active_sessions", &heartbeat->active_sessions);
  ParseIntField(fields, "max_active_sessions", &heartbeat->max_active_sessions);
  ParseUint64Field(fields, "completed_sessions", &heartbeat->completed_sessions);
  ParseUint64Field(fields, "failed_sessions", &heartbeat->failed_sessions);
  ParseBoolField(fields, "healthy", &heartbeat->healthy);

  const auto supports = fields.find("supports");
  if (supports != fields.end()) {
    heartbeat->supported_networks = orchestration::SplitNetworks(supports->second);
  }
}

void PrintUsage(const char *program) {
  std::cerr << "Usage: " << program << " node_id=<id> node_role=server|client "
            << "[server_ip=<ip>] [backend=cheetah|sci-he] "
            << "[control_port=<port>] [status_port=<port>] "
            << "[idle_power_w=<watts>] [power_path=<sysfs-path>] "
            << "[allow_no_power_sensor=0|1]"
            << std::endl;
}

}  // namespace

int main(int argc, char **argv) {
  try {
    Config config = ParseConfig(argc, argv);
    std::unique_ptr<orchestration::PowerSensor> power_sensor =
        orchestration::CreatePowerSensor(config.power_path, config.allow_no_power_sensor);
    orchestration::NodeHeartbeat heartbeat = MakeBaseHeartbeat(config);
    ReadPower(config, power_sensor.get(), &heartbeat);

    if (config.node_role == orchestration::NodeRole::kServer) {
      try {
        PollServerStatus(config, &heartbeat);
      } catch (const std::exception &e) {
        heartbeat.healthy = false;
        std::cerr << "[node-reporter] status poll failed: " << e.what() << std::endl;
      }
    }

    std::cout << orchestration::SerializeHeartbeat(heartbeat);
  } catch (const std::exception &e) {
    std::cerr << "[node-reporter] " << e.what() << std::endl;
    PrintUsage(argv[0]);
    return 1;
  }

  return 0;
}
