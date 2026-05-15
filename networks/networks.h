#ifndef NETWORKS_NETWORKS_H_
#define NETWORKS_NETWORKS_H_

#include <cstdint>
#include <string>

void run_sqnet_inference(int party_, int port_, const std::string &address_,
                         int num_threads_, int32_t bitlength_, int32_t kScale_);

#endif  // NETWORKS_NETWORKS_H_
