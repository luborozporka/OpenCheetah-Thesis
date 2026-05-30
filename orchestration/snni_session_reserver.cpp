#include "orchestration/snni_session_reserver.h"

#include <asio.hpp>

#include <cstdint>
#include <cstring>
#include <string>

namespace orchestration {
namespace {

enum class ControlNetworkId : uint32_t {
  kSqnet = 1,
  kResnet50 = 2,
  kDensenet121 = 3,
};

enum class ControlStatus : uint32_t {
  kOk = 0,
  kBadMagic = 1,
  kUnknownNetwork = 2,
  kBadConfig = 3,
  kModelUnavailable = 4,
};

struct ControlRequest {
  char magic[4];
  ControlNetworkId network_id;
  int32_t bitlength;
  int32_t scale;
  int32_t num_threads;
};

struct ControlResponse {
  ControlStatus status;
  uint32_t data_port;
};

static_assert(sizeof(ControlRequest) == 20, "SNNI session request layout changed");
static_assert(sizeof(ControlResponse) == 8, "SNNI session response layout changed");

bool ToControlNetworkId(Network network, ControlNetworkId *out) {
  switch (network) {
    case Network::kSqnet:
      *out = ControlNetworkId::kSqnet;
      return true;
    case Network::kResnet50:
      *out = ControlNetworkId::kResnet50;
      return true;
    case Network::kDensenet121:
      *out = ControlNetworkId::kDensenet121;
      return true;
    case Network::kUnknown:
      return false;
  }
  return false;
}

std::string ToString(ControlStatus status) {
  switch (status) {
    case ControlStatus::kOk:
      return "OK";
    case ControlStatus::kBadMagic:
      return "BadMagic";
    case ControlStatus::kUnknownNetwork:
      return "UnknownNetwork";
    case ControlStatus::kBadConfig:
      return "BadConfig";
    case ControlStatus::kModelUnavailable:
      return "ModelUnavailable";
  }
  return "UnknownStatus";
}

SnniSessionReservation ErrorResult(const std::string &error) {
  SnniSessionReservation result;
  result.error = error;
  return result;
}

}  // namespace

SnniSessionReservation ReserveSnniSession(const std::string &server_ip,
                                          uint16_t control_port,
                                          const RoutingRequest &request) {
  ControlNetworkId network_id = ControlNetworkId::kSqnet;
  if (!ToControlNetworkId(request.network, &network_id)) {
    return ErrorResult("unsupported network for SNNI session request");
  }

  try {
    ControlRequest control_request{};
    std::memcpy(control_request.magic, "SNNI", 4);
    control_request.network_id = network_id;
    control_request.bitlength = request.bitlength;
    control_request.scale = request.scale;
    control_request.num_threads = request.num_threads;

    asio::io_context io;
    asio::ip::tcp::resolver resolver(io);
    asio::ip::tcp::socket socket(io);
    asio::connect(socket, resolver.resolve(server_ip, std::to_string(control_port)));
    asio::write(socket, asio::buffer(&control_request, sizeof(control_request)));

    ControlResponse control_response{};
    asio::read(socket, asio::buffer(&control_response, sizeof(control_response)));

    if (control_response.status != ControlStatus::kOk) {
      return ErrorResult("SNNI session request rejected: " + ToString(control_response.status));
    }
    if (control_response.data_port == 0 || control_response.data_port > UINT16_MAX) {
      return ErrorResult("SNNI session returned invalid data port");
    }

    SnniSessionReservation result;
    result.ok = true;
    result.data_port = static_cast<uint16_t>(control_response.data_port);
    return result;
  } catch (const std::exception &e) {
    return ErrorResult("SNNI session reservation failed: " + std::string(e.what()));
  }
}

}  // namespace orchestration
