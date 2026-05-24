#include <asio.hpp>

#include "orchestration/common/protocol.h"
#include "orchestration/common/protocol_internal.h"
#include "orchestration/common/time_utils.h"
#include "orchestration/node_registry.h"

#include <cstdint>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

struct Config {
  uint16_t heartbeat_port = 18080;
  int64_t heartbeat_timeout_ms = 10000;
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

void HandleHeartbeat(asio::ip::tcp::socket socket,
                     orchestration::NodeRegistry *registry,
                     int64_t heartbeat_timeout_ms) {
  try {
    const std::string message = ReadTcpMessage(socket);

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

    std::cout << "[orchestrator] " << (inserted ? "registered " : "updated ")
              << heartbeat.node_id << " role="
              << orchestration::ToString(heartbeat.node_role)
              << " healthy=" << (heartbeat.healthy ? 1 : 0)
              << " active=" << heartbeat.active_sessions << '/'
              << heartbeat.max_active_sessions << " nodes=["
              << JoinNodeIds(snapshot) << "]" << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "[orchestrator] heartbeat error: " << e.what() << std::endl;
  }
}

void PrintUsage(const char *program) {
  std::cerr << "Usage: " << program << " [p=<heartbeat_port>] [heartbeat_timeout_ms=<milliseconds>]" << std::endl;
}

}  // namespace

int main(int argc, char **argv) {
  try {
    const Config config = ParseConfig(argc, argv);
    orchestration::NodeRegistry registry;

    asio::io_context io;
    asio::ip::tcp::acceptor acceptor(io, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), config.heartbeat_port));
    std::cout << "[orchestrator] heartbeat listening on " << config.heartbeat_port << std::endl;

    while (true) {
      asio::ip::tcp::socket socket(io);
      acceptor.accept(socket);
      std::thread(HandleHeartbeat, std::move(socket), &registry, config.heartbeat_timeout_ms).detach();
    }
  } catch (const std::exception &e) {
    std::cerr << "[orchestrator] " << e.what() << std::endl;
    PrintUsage(argv[0]);
    return 1;
  }
}
