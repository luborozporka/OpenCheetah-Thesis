#include "globals.h"
#include "networks/networks.h"
#include "utils/ArgMapping/ArgMapping.h"

#include <unistd.h>

#include <cstdint>
#include <string>

int main(int argc, char **argv) {
  int party = 0;
  int port = 32000;
  std::string address = "127.0.0.1";
  int num_threads = 4;
  int32_t bitlength = 41;
  int32_t kScale = 12;

  ArgMapping amap;

  amap.arg("r", party, "Role of party: ALICE/SERVER = 1; BOB/CLIENT = 2");
  amap.arg("p", port, "Port Number");
  amap.arg("ip", address, "IP Address of server (ALICE)");
  amap.arg("nt", num_threads, "Number of Threads");
  amap.arg("ell", bitlength, "Uniform Bitwidth");
  amap.arg("k", kScale, "bits of scale");

  amap.parse(argc, argv);

  g_session_tag = "p" + std::to_string(port) + "_pid" + std::to_string(getpid());

  run_resnet50_inference(party, port, address, num_threads, bitlength, kScale);
}
