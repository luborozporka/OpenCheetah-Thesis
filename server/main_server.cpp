#include <asio.hpp>

#include <unistd.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

#include "SCI/src/globals.h"
#include "SCI/src/session.h"
#include "networks/networks.h"
#include "utils/ArgMapping/ArgMapping.h"

namespace {

constexpr char kMagic[4] = {'S', 'N', 'N', 'I'};

enum class NetworkId : uint32_t {
  Sqnet = 1,
  Resnet50 = 2,
  Densenet121 = 3,
};

enum class Status : uint32_t {
  OK = 0,
  BadMagic = 1,
  UnknownNetwork = 2,
  BadConfig = 3,
  ModelUnavailable = 4,
};

struct Request {
  char magic[4];
  NetworkId network_id;
  int32_t bitlength;
  int32_t scale;
  int32_t num_threads;
};

struct Response {
  Status status;
  uint32_t data_port;
};

using InferenceFn = void (*)(int, int, const std::string &, int, int32_t, int32_t, std::istream &);

// Model weights are loaded only once at startup,
// and are then shared with worker threads
std::string g_sqnet_weights;
std::string g_resnet50_weights;
std::string g_densenet121_weights;

// Atomic port allocator
std::atomic<int> g_next_data_port{0};

// Status counters
std::atomic<int> g_active_sessions{0};
std::atomic<uint64_t> g_completed_sessions{0};
std::atomic<uint64_t> g_failed_sessions{0};

struct ActiveSessionGuard {
  ActiveSessionGuard() { g_active_sessions.fetch_add(1, std::memory_order_relaxed); }

  ActiveSessionGuard(const ActiveSessionGuard &) = delete;
  ActiveSessionGuard &operator=(const ActiveSessionGuard &) = delete;

  ~ActiveSessionGuard() {
    if (completed_) {
      g_completed_sessions.fetch_add(1, std::memory_order_relaxed);
    } else {
      g_failed_sessions.fetch_add(1, std::memory_order_relaxed);
    }
    g_active_sessions.fetch_sub(1, std::memory_order_relaxed);
  }

  void mark_completed() { completed_ = true; }

 private:
  bool completed_ = false;
};

void send_response(asio::ip::tcp::socket &socket, Status status, uint32_t data_port) {
  Response resp{status, data_port};
  asio::write(socket, asio::buffer(&resp, sizeof(resp)));
}

void handle_client(asio::ip::tcp::socket socket) {
  std::string peer;
  try {
    peer = socket.remote_endpoint().address().to_string();

    Request req{};
    asio::read(socket, asio::buffer(&req, sizeof(req)));

    if (std::memcmp(req.magic, kMagic, sizeof(kMagic)) != 0) {
      send_response(socket, Status::BadMagic, 0);
      return;
    }

    const char *network_name = nullptr;
    const std::string *weights = nullptr;
    InferenceFn inference_fn = nullptr;
    switch (req.network_id) {
      case NetworkId::Sqnet:
        network_name = "sqnet";
        weights = &g_sqnet_weights;
        inference_fn = &run_sqnet_inference;
        break;
      case NetworkId::Resnet50:
        network_name = "resnet50";
        weights = &g_resnet50_weights;
        inference_fn = &run_resnet50_inference;
        break;
      case NetworkId::Densenet121:
        network_name = "densenet121";
        weights = &g_densenet121_weights;
        inference_fn = &run_densenet121_inference;
        break;
      default:
        send_response(socket, Status::UnknownNetwork, 0);
        return;
    }

    if (weights->empty()) {
      send_response(socket, Status::ModelUnavailable, 0);
      return;
    }
    if (req.num_threads <= 0 || req.num_threads > MAX_THREADS) {
      send_response(socket, Status::BadConfig, 0);
      return;
    }

    const uint32_t data_port = static_cast<uint32_t>(g_next_data_port.fetch_add(req.num_threads));
    send_response(socket, Status::OK, data_port);
    socket.close();
    ActiveSessionGuard session_guard;

    std::cout << "[server] " << peer << " -> " << network_name << " on port "
              << data_port << std::endl;

    g_session_tag = "w" + std::to_string(data_port) + "_pid" + std::to_string(getpid());

    std::istringstream weights_in(*weights);
    inference_fn(/*party=*/1, /*port=*/static_cast<int>(data_port),
                 /*address=*/"127.0.0.1", req.num_threads,
                 req.bitlength, req.scale, weights_in);

    std::cout << "[server] " << peer << " done (port " << data_port << ")"
              << std::endl;
    session_guard.mark_completed();
  } catch (const std::exception &e) {
    std::cerr << "[server] worker error (" << peer << "): " << e.what()
              << std::endl;
  }
}

std::string read_file(const std::string &path) {
  std::ifstream f(path, std::ios::in);
  if (!f) throw std::runtime_error("cannot open " + path);
  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

}  // namespace

int main(int argc, char **argv) {
  int port = 12345;
  int data_port_base = 0;
  std::string sqnet_weights_path;
  std::string resnet50_weights_path;
  std::string densenet121_weights_path;

  ArgMapping amap;
  amap.arg("p", port, "Control port");
  amap.arg("dp", data_port_base, "Base port for data channels (default: p+1)");
  amap.arg("sqnet_weights", sqnet_weights_path, "Path to sqnet weights file");
  amap.arg("resnet50_weights", resnet50_weights_path, "Path to resnet50 weights file");
  amap.arg("densenet121_weights", densenet121_weights_path, "Path to densenet121 weights file");
  amap.parse(argc, argv);

  if (sqnet_weights_path.empty()
      && resnet50_weights_path.empty()
      && densenet121_weights_path.empty()) {
    std::cerr << "[server] at least one of sqnet_weights=..., resnet50_weights=..., or densenet121_weights=... is required"
              << std::endl;
    return 1;
  }
  if (data_port_base == 0) data_port_base = port + 1;
  g_next_data_port.store(data_port_base);

  try {
    if (!sqnet_weights_path.empty()) {
      g_sqnet_weights = read_file(sqnet_weights_path);
      std::cout << "[server] loaded " << g_sqnet_weights.size()
                << " bytes from " << sqnet_weights_path << " (sqnet)"
                << std::endl;
    }
    if (!resnet50_weights_path.empty()) {
      g_resnet50_weights = read_file(resnet50_weights_path);
      std::cout << "[server] loaded " << g_resnet50_weights.size()
                << " bytes from " << resnet50_weights_path << " (resnet50)"
                << std::endl;
    }
    if (!densenet121_weights_path.empty()) {
      g_densenet121_weights = read_file(densenet121_weights_path);
      std::cout << "[server] loaded " << g_densenet121_weights.size()
                << " bytes from " << densenet121_weights_path << " (densenet121)"
                << std::endl;
    }
  } catch (const std::exception &e) {
    std::cerr << "[server] " << e.what() << std::endl;
    return 1;
  }

  try {
    asio::io_context io;
    asio::ip::tcp::acceptor acceptor(io, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port));
    std::cout << "[server] listening on " << port << " (data ports from "
              << data_port_base << ")" << std::endl;

    while (true) {
      asio::ip::tcp::socket socket(io);
      acceptor.accept(socket);
      std::thread(handle_client, std::move(socket)).detach();
    }
  } catch (const std::exception &e) {
    std::cerr << "[server] fatal: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
