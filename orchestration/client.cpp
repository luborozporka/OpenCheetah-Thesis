#include <asio.hpp>

#include "orchestration/common/protocol.h"
#include "orchestration/common/protocol_internal.h"
#include "orchestration/common/time_utils.h"

#include <array>
#include <cstdio>
#include <cstdint>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/wait.h>

namespace {

struct Config {
  std::string orchestrator_ip = "127.0.0.1";
  uint16_t orchestrator_port = 18080;
  std::string client_binary;
  std::string binary_dir = "build/bin";
  std::string input_path;
  int64_t expected_label = -1;
  orchestration::RoutingRequest request;
};

struct ClientRunResult {
  int exit_code = -1;
  bool label_found = false;
  int64_t predicted_label = -1;
  std::string output;
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
  int64_t parsed = 0;
  if (!orchestration::protocol_internal::ParseInt64(value, &parsed)) {
    throw std::runtime_error(field_name + " must be an integer");
  }
  if (parsed <= 0 || parsed > 65535) {
    throw std::runtime_error(field_name + " must be in range 1..65535");
  }
  return static_cast<uint16_t>(parsed);
}

int32_t ParseInt32(const std::string &value,
                   const std::string &field_name,
                   int32_t default_value) {
  if (value.empty()) return default_value;
  int64_t parsed = 0;
  if (!orchestration::protocol_internal::ParseInt64(value, &parsed)) {
    throw std::runtime_error(field_name + " must be an integer");
  }
  return static_cast<int32_t>(parsed);
}

int64_t ParseInt64(const std::string &value,
                   const std::string &field_name,
                   int64_t default_value) {
  if (value.empty()) return default_value;
  int64_t parsed = 0;
  if (!orchestration::protocol_internal::ParseInt64(value, &parsed)) {
    throw std::runtime_error(field_name + " must be an integer");
  }
  return parsed;
}

double ParseDouble(const std::string &value,
                   const std::string &field_name,
                   double default_value) {
  if (value.empty()) return default_value;
  double parsed = 0.0;
  if (!orchestration::protocol_internal::ParseDouble(value, &parsed)) {
    throw std::runtime_error(field_name + " must be a number");
  }
  return parsed;
}

std::string DefaultRequestId() {
  return "req-" + std::to_string(orchestration::NowMillis());
}

int32_t DefaultBitlength(orchestration::Network network) {
  return network == orchestration::Network::kSqnet ? 32 : 41;
}

std::string BackendExecutableSuffix(orchestration::Backend backend) {
  switch (backend) {
    case orchestration::Backend::kCheetah:
      return "cheetah";
    case orchestration::Backend::kSciHe:
      return "SCI_HE";
    case orchestration::Backend::kUnknown:
      break;
  }
  throw std::runtime_error("unsupported backend for client binary");
}

std::string DefaultClientBinary(const Config &config) {
  return config.binary_dir + "/" + orchestration::ToString(config.request.network) + "-" + BackendExecutableSuffix(config.request.backend);
}

std::string ShellQuote(const std::string &value) {
  std::string quoted = "'";
  for (char ch : value) {
    if (ch == '\'') {
      quoted += "'\\''";
    } else {
      quoted += ch;
    }
  }
  quoted += "'";
  return quoted;
}

Config ParseConfig(int argc, char **argv) {
  const auto args = ParseArgs(argc, argv);

  Config config;
  config.orchestrator_ip = GetArg(args, "orchestrator_ip", config.orchestrator_ip);
  const uint16_t orchestrator_port = ParsePort(GetArg(args, "orchestrator_port"), "orchestrator_port");
  if (orchestrator_port != 0) config.orchestrator_port = orchestrator_port;

  if (!orchestration::ParseBackend(GetArg(args, "backend"), &config.request.backend)) {
    throw std::runtime_error("backend must be cheetah or sci-he");
  }
  if (!orchestration::ParseNetwork(GetArg(args, "network"), &config.request.network)) {
    throw std::runtime_error("network must be sqnet, resnet50, or densenet121");
  }

  config.request.request_id = GetArg(args, "request_id", DefaultRequestId());
  config.request.input_shape = GetArg(args, "input_shape");
  config.request.bitlength = ParseInt32(GetArg(args, "ell"), "ell", DefaultBitlength(config.request.network));
  config.request.scale = ParseInt32(GetArg(args, "k"), "k", 12);
  config.request.num_threads = ParseInt32(GetArg(args, "nt"), "nt", 4);
  config.request.max_latency_ms = ParseInt64(GetArg(args, "max_latency_ms"), "max_latency_ms", -1);
  config.request.min_accuracy = ParseDouble(GetArg(args, "min_accuracy"), "min_accuracy", -1.0);

  config.input_path = GetArg(args, "input");
  if (config.input_path.empty()) {
    throw std::runtime_error("input=<path> is required");
  }
  config.expected_label = ParseInt64(GetArg(args, "expected_label"), "expected_label", -1);
  config.client_binary = GetArg(args, "client_binary");
  config.binary_dir = GetArg(args, "binary_dir", config.binary_dir);

  return config;
}

std::string ReadTcpMessage(asio::ip::tcp::socket &socket) {
  std::string message;
  std::array<char, 4096> buffer{};
  asio::error_code ec;
  while (true) {
    const size_t bytes = socket.read_some(asio::buffer(buffer), ec);
    if (bytes > 0) message.append(buffer.data(), bytes);
    if (ec == asio::error::eof) break;
    if (ec) throw asio::system_error(ec);
  }
  return message;
}

orchestration::RoutingResponse RequestRouting(const Config &config) {
  asio::io_context io;
  asio::ip::tcp::resolver resolver(io);
  asio::ip::tcp::socket socket(io);
  asio::connect(socket, resolver.resolve(config.orchestrator_ip, std::to_string(config.orchestrator_port)));

  const std::string payload = orchestration::SerializeRoutingRequest(config.request);
  asio::write(socket, asio::buffer(payload.data(), payload.size()));
  socket.shutdown(asio::ip::tcp::socket::shutdown_send);

  orchestration::RoutingResponse response;
  std::string error;
  if (!orchestration::ParseRoutingResponse(ReadTcpMessage(socket), &response, &error)) {
    throw std::runtime_error("invalid routing response: " + error);
  }
  return response;
}

std::string BuildStandaloneCommand(const Config &config, const orchestration::RoutingResponse &response) {
  const std::string binary = config.client_binary.empty()
    ? DefaultClientBinary(config)
    : config.client_binary;
  std::ostringstream command;
  command << ShellQuote(binary)
          << " r=2"
          << " ip=" << ShellQuote(response.server_ip)
          << " p=" << response.data_port
          << " k=" << config.request.scale
          << " ell=" << config.request.bitlength
          << " nt=" << config.request.num_threads
          << " < " << ShellQuote(config.input_path)
          << " 2>&1";
  return command.str();
}

int DecodeExitCode(int status) {
  if (status == -1) return -1;
  if (WIFEXITED(status)) return WEXITSTATUS(status);
  if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
  return status;
}

ClientRunResult RunStandaloneClient(
    const Config &config,
    const orchestration::RoutingResponse &response) {
  ClientRunResult result;
  const std::string command = BuildStandaloneCommand(config, response);
  FILE *pipe = popen(command.c_str(), "r");
  if (pipe == nullptr) {
    throw std::runtime_error("failed to launch standalone client");
  }

  std::array<char, 4096> buffer{};
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
    result.output += buffer.data();
  }
  result.exit_code = DecodeExitCode(pclose(pipe));

  const std::regex label_regex("predicted label\\s*=\\s*([0-9]+)");
  std::smatch match;
  if (std::regex_search(result.output, match, label_regex)) {
    result.label_found = true;
    result.predicted_label = std::stoll(match[1].str());
  }

  return result;
}

void PrintUsage(const char *program) {
  std::cerr
      << "Usage: " << program
      << " backend=<cheetah|sci-he> network=<sqnet|resnet50|densenet121>"
      << " input=<path> [expected_label=<label>]"
      << " [orchestrator_ip=<ip>] [orchestrator_port=<port>]"
      << " [client_binary=<path>|binary_dir=<dir>]"
      << " [ell=<bitlength>] [k=<scale>] [nt=<threads>]"
      << " [max_latency_ms=<ms>] [min_accuracy=<value>]"
      << std::endl;
}

}  // namespace

int main(int argc, char **argv) {
  try {
    const Config config = ParseConfig(argc, argv);
    const orchestration::RoutingResponse response = RequestRouting(config);
    if (response.status != orchestration::RoutingStatus::kOk) {
      std::cerr << "[client] routing failed status=" << orchestration::ToString(response.status)
                << " reason=" << response.reason
                << std::endl;
      return 2;
    }

    const ClientRunResult result = RunStandaloneClient(config, response);
    std::cout << result.output;

    const bool expected_ok = config.expected_label < 0 || result.predicted_label == config.expected_label;
    const bool success = result.exit_code == 0 && result.label_found && expected_ok;
    std::cout << "[client] request_id=" << config.request.request_id
              << " node_id=" << response.node_id
              << " server_ip=" << response.server_ip
              << " data_port=" << response.data_port
              << " exit_code=" << result.exit_code
              << " label_found=" << (result.label_found ? 1 : 0)
              << " predicted_label=" << result.predicted_label
              << " expected_label=" << config.expected_label
              << " success=" << (success ? 1 : 0)
              << std::endl;
    return success ? 0 : 1;
  } catch (const std::exception &e) {
    std::cerr << "[client] " << e.what() << std::endl;
    PrintUsage(argv[0]);
    return 1;
  }
}
