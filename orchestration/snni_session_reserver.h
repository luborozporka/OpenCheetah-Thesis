#ifndef ORCHESTRATION_SNNI_SESSION_RESERVER_H_
#define ORCHESTRATION_SNNI_SESSION_RESERVER_H_

#include "orchestration/common/protocol.h"

#include <cstdint>
#include <string>

namespace orchestration {

struct SnniSessionReservation {
  bool ok = false;
  uint16_t data_port = 0;
  std::string error;
};

SnniSessionReservation ReserveSnniSession(const std::string &server_ip,
                                          uint16_t control_port,
                                          const RoutingRequest &request);

}

#endif