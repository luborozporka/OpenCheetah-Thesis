#include <asio.hpp>

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
};

enum class Status : uint32_t {
  OK = 0,
  BadMagic = 1,
  UnknownNetwork = 2,
  BadConfig = 3,
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

// Model weights are loaded only once at startup,
// and are then shared with worker threads
std::string g_weights;

// Atomic port allocator
std::atomic<int> g_next_data_port{0};

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
    if (req.network_id != NetworkId::Sqnet) {
      send_response(socket, Status::UnknownNetwork, 0);
      return;
    }
    if (req.num_threads <= 0 || req.num_threads > MAX_THREADS) {
      send_response(socket, Status::BadConfig, 0);
      return;
    }

    const uint32_t data_port = static_cast<uint32_t>(g_next_data_port.fetch_add(req.num_threads));
    send_response(socket, Status::OK, data_port);
    socket.close();

    std::cout << "[server] " << peer << " -> sqnet on port " << data_port
              << std::endl;

    g_session_tag = "w" + std::to_string(data_port);

    std::istringstream weights_in(g_weights);
    run_sqnet_inference(/*party=*/1, /*port=*/static_cast<int>(data_port),
                        /*address=*/"127.0.0.1", req.num_threads,
                        req.bitlength, req.scale, weights_in);

    std::cout << "[server] " << peer << " done (port " << data_port << ")"
              << std::endl;
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
  std::string weights_path;

  ArgMapping amap;
  amap.arg("p", port, "Control port");
  amap.arg("dp", data_port_base, "Base port for data channels (default: p+1)");
  amap.arg("weights", weights_path, "Path to model weights file");
  amap.parse(argc, argv);

  if (weights_path.empty()) {
    std::cerr << "[server] weights=... is required" << std::endl;
    return 1;
  }
  if (data_port_base == 0) data_port_base = port + 1;
  g_next_data_port.store(data_port_base);

  try {
    g_weights = read_file(weights_path);
    std::cout << "[server] loaded " << g_weights.size() << " bytes from "
              << weights_path << std::endl;
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
