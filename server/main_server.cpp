#include <asio.hpp>

#include <exception>
#include <iostream>
#include <thread>
#include <utility>

#include "utils/ArgMapping/ArgMapping.h"

namespace {

int port = 12345;

void handle_client(asio::ip::tcp::socket socket) {
  try {
    const auto endpoint = socket.remote_endpoint();
    std::cout << "[server] client connected from "
              << endpoint.address().to_string() << ":" << endpoint.port()
              << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "[server] worker error: " << e.what() << std::endl;
  }
}

}  // namespace

int main(int argc, char **argv) {
  ArgMapping amap;
  amap.arg("p", port, "Control port");
  amap.parse(argc, argv);

  try {
    asio::io_context io;
    asio::ip::tcp::acceptor acceptor(io, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port));
    std::cout << "[server] listening on port " << port << std::endl;

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
