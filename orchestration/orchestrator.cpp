#include <asio.hpp>

#include "orchestration/common/csv_log.h"
#include "orchestration/common/protocol.h"
#include "orchestration/common/protocol_internal.h"
#include "orchestration/common/time_utils.h"
#include "orchestration/node_registry.h"
#include "orchestration/routing_policy.h"
#include "orchestration/snni_session_reserver.h"

#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

struct Config {
  uint16_t heartbeat_port = 18080;
  int64_t heartbeat_timeout_ms = 10000;
  std::string node_metrics_log_path;
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

std::string GetArg(const std::map<std::string, std::string> &args,
                   const std::string &key,
                   const std::string &default_value = "") {
  const auto it = args.find(key);
  return it == args.end() ? default_value : it->second;
}

uint16_t ParsePort(const std::string &value, const std::string &field_name) {
  if (value.empty()) return 0;
  int64_t port = 0;
  if (!orchestration::protocol_internal::ParseInt64(value, &port)) {
    throw std::runtime_error(field_name + " must be an integer");
  }
  if (port <= 0 || port > 65535) {
    throw std::runtime_error(field_name + " must be in range 1..65535");
  }
  return static_cast<uint16_t>(port);
}

int64_t ParseInt64Arg(const std::string &value, const std::string &field_name) {
  if (value.empty()) return 0;
  int64_t result = 0;
  if (!orchestration::protocol_internal::ParseInt64(value, &result)) {
    throw std::runtime_error(field_name + " must be an integer");
  }
  return result;
}

Config ParseConfig(int argc, char **argv) {
  const auto args = ParseArgs(argc, argv);

  Config config;
  const uint16_t port = ParsePort(GetArg(args, "p"), "p");
  if (port != 0) config.heartbeat_port = port;

  const int64_t timeout = ParseInt64Arg(GetArg(args, "heartbeat_timeout_ms"), "heartbeat_timeout_ms");
  if (timeout != 0) config.heartbeat_timeout_ms = timeout;
  if (config.heartbeat_timeout_ms < 0) {
    throw std::runtime_error("heartbeat_timeout_ms must be non-negative");
  }
  config.node_metrics_log_path = GetArg(args, "node_metrics_log");

  return config;
}

std::string ReadTcpMessage(asio::ip::tcp::socket &socket) {
  std::string message;
  char buffer[4096];
  asio::error_code ec;
  while (true) {
    const size_t bytes = socket.read_some(asio::buffer(buffer), ec);
    if (bytes > 0) message.append(buffer, bytes);
    if (ec == asio::error::eof) break;
    if (ec) throw asio::system_error(ec);
  }
  return message;
}

std::string JoinNodeIds(const std::vector<orchestration::NodeHeartbeat> &nodes) {
  std::string result;
  for (const auto &node : nodes) {
    if (!result.empty()) result += ',';
    result += node.node_id;
  }
  return result;
}

std::string BoolString(bool value) { return value ? "1" : "0"; }

std::string MessageType(const std::string &message) {
  const orchestration::KeyValueMessage fields = orchestration::ParseKeyValueMessage(message);
  const auto it = fields.find("type");
  return it == fields.end() ? "" : it->second;
}

void WriteNodeMetrics(orchestration::CsvLog *log,
                      std::mutex *mutex,
                      const orchestration::NodeHeartbeat &heartbeat,
                      int64_t received_at_ms) {
  if (log == nullptr) return;

  const int64_t timestamp_ms =
      heartbeat.timestamp_ms != 0 ? heartbeat.timestamp_ms : received_at_ms;
  std::lock_guard<std::mutex> lock(*mutex);
  log->WriteRow({
      std::to_string(timestamp_ms),
      heartbeat.node_id,
      orchestration::ToString(heartbeat.node_role),
      orchestration::ToString(heartbeat.backend),
      BoolString(heartbeat.power_available),
      std::to_string(heartbeat.power_w),
      std::to_string(heartbeat.idle_power_w),
      std::to_string(heartbeat.dynamic_power_w),
      std::to_string(heartbeat.active_sessions),
      std::to_string(heartbeat.max_active_sessions),
      std::to_string(heartbeat.mem_available_bytes),
      BoolString(heartbeat.healthy),
  });
}

void HandleHeartbeatMessage(const std::string &message,
                            orchestration::NodeRegistry *registry,
                            orchestration::CsvLog *node_metrics_log,
                            std::mutex *node_metrics_log_mutex,
                            int64_t heartbeat_timeout_ms) {
  orchestration::NodeHeartbeat heartbeat;
  std::string error;
  if (!orchestration::ParseHeartbeat(message, &heartbeat, &error)) {
    std::cerr << "[orchestrator] rejected heartbeat: " << error << std::endl;
    return;
  }
  if (heartbeat.node_id.empty()) {
    std::cerr << "[orchestrator] rejected heartbeat: empty node_id" << std::endl;
    return;
  }

  const int64_t now_ms = orchestration::NowMillis();
  const bool inserted = registry->RecordHeartbeat(heartbeat, now_ms);
  const auto snapshot = registry->Snapshot(now_ms, heartbeat_timeout_ms);
  WriteNodeMetrics(node_metrics_log, node_metrics_log_mutex, heartbeat, now_ms);

  std::cout << "[orchestrator] "
            << (inserted ? "registered " : "updated ") << heartbeat.node_id
            << " role=" << orchestration::ToString(heartbeat.node_role)
            << " healthy=" << (heartbeat.healthy ? 1 : 0)
            << " active=" << heartbeat.active_sessions << '/' << heartbeat.max_active_sessions << " nodes=[" << JoinNodeIds(snapshot) << "]"
            << std::endl;
}

void WriteRoutingResponse(asio::ip::tcp::socket &socket, const orchestration::RoutingResponse &response) {
  const std::string payload = orchestration::SerializeRoutingResponse(response);
  asio::write(socket, asio::buffer(payload.data(), payload.size()));
}

void HandleRoutingRequestMessage(asio::ip::tcp::socket &socket,
                                 const std::string &message,
                                 orchestration::NodeRegistry *registry,
                                 orchestration::RoutingPolicyState *routing_state,
                                 int64_t heartbeat_timeout_ms) {
  orchestration::RoutingRequest request;
  std::string error;
  if (!orchestration::ParseRoutingRequest(message, &request, &error)) {
    const orchestration::KeyValueMessage fields = orchestration::ParseKeyValueMessage(message);
    const auto request_id = fields.find("request_id");
    orchestration::RoutingResponse response;
    response.status = orchestration::RoutingStatus::kInvalidRequest;
    response.request_id = request_id == fields.end() ? "" : request_id->second;
    response.reason = error;
    WriteRoutingResponse(socket, response);
    std::cerr << "[orchestrator] rejected routing request: " << error << std::endl;
    return;
  }

  const int64_t now_ms = orchestration::NowMillis();
  const auto snapshot = registry->Snapshot(now_ms, heartbeat_timeout_ms);
  const auto decision =
      orchestration::SelectRoutingCandidate(snapshot, request, routing_state);

  orchestration::RoutingResponse response;
  response.request_id = request.request_id;
  response.backend = request.backend;
  response.network = request.network;
  if (!decision.ok) {
    response.status = decision.candidates.empty()
      ? orchestration::RoutingStatus::kNoCapacity
      : orchestration::RoutingStatus::kInvalidRequest;
    response.reason = decision.reason;
  } else {
    const orchestration::NodeHeartbeat selected = decision.selected;
    response.node_id = selected.node_id;
    response.server_ip = selected.server_ip;
    const orchestration::SnniSessionReservation reservation =
        orchestration::ReserveSnniSession(selected.server_ip, selected.control_port, request);
    if (reservation.ok) {
      response.status = orchestration::RoutingStatus::kOk;
      response.data_port = reservation.data_port;
    } else {
      response.status = orchestration::RoutingStatus::kServerError;
      response.reason = reservation.error;
    }
  }
  WriteRoutingResponse(socket, response);

  std::cout << "[orchestrator] routing request " << request.request_id
            << " policy=" << orchestration::ToString(request.policy)
            << " backend=" << orchestration::ToString(request.backend)
            << " network=" << orchestration::ToString(request.network)
            << " candidates=" << decision.candidates.size()
            << " selected=" << response.node_id
            << " status=" << orchestration::ToString(response.status)
            << std::endl;
}

void HandleConnection(asio::ip::tcp::socket socket,
                      orchestration::NodeRegistry *registry,
                      orchestration::RoutingPolicyState *routing_state,
                      orchestration::CsvLog *node_metrics_log,
                      std::mutex *node_metrics_log_mutex,
                      int64_t heartbeat_timeout_ms) {
  try {
    const std::string message = ReadTcpMessage(socket);
    const std::string type = MessageType(message);
    if (type == "heartbeat") {
      HandleHeartbeatMessage(message, registry, node_metrics_log, node_metrics_log_mutex, heartbeat_timeout_ms);
    } else if (type == "routing_request") {
      HandleRoutingRequestMessage(socket, message, registry, routing_state, heartbeat_timeout_ms);
    } else {
      std::cerr << "[orchestrator] rejected message: unknown type" << std::endl;
    }
  } catch (const std::exception &e) {
    std::cerr << "[orchestrator] connection error: " << e.what() << std::endl;
  }
}

void PrintUsage(const char *program) {
  std::cerr << "Usage: " << program
            << " [p=<heartbeat_port>] [heartbeat_timeout_ms=<milliseconds>]"
            << " [node_metrics_log=<path>]" << std::endl;
}

}  // namespace

int main(int argc, char **argv) {
  try {
    const Config config = ParseConfig(argc, argv);
    orchestration::NodeRegistry registry;
    orchestration::RoutingPolicyState routing_state;
    std::unique_ptr<orchestration::CsvLog> node_metrics_log;
    std::mutex node_metrics_log_mutex;
    if (!config.node_metrics_log_path.empty()) {
      node_metrics_log = std::make_unique<orchestration::CsvLog>(
          config.node_metrics_log_path,
          std::vector<std::string>{
              "timestamp_ms",
              "node_id",
              "node_role",
              "backend",
              "power_available",
              "power_w",
              "idle_power_w",
              "dynamic_power_w",
              "active_sessions",
              "max_active_sessions",
              "mem_available_bytes",
              "healthy",
          });
    }

    asio::io_context io;
    asio::ip::tcp::acceptor acceptor(io, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), config.heartbeat_port));
    std::cout << "[orchestrator] listening on " << config.heartbeat_port << std::endl;

    while (true) {
      asio::ip::tcp::socket socket(io);
      acceptor.accept(socket);
      std::thread(HandleConnection, std::move(socket), &registry, &routing_state,
                  node_metrics_log.get(), &node_metrics_log_mutex, config.heartbeat_timeout_ms)
          .detach();
    }
  } catch (const std::exception &e) {
    std::cerr << "[orchestrator] " << e.what() << std::endl;
    PrintUsage(argv[0]);
    return 1;
  }
}
