#ifndef NETWORKS_NETWORKS_H_
#define NETWORKS_NETWORKS_H_

#include <cstdint>
#include <iostream>
#include <string>

extern thread_local int32_t kScale;

void run_sqnet_inference(int party_, int port_, const std::string &address_,
                         int num_threads_, int32_t bitlength_, int32_t kScale_,
                         std::istream &in = std::cin);

void run_resnet50_inference(int party_, int port_, const std::string &address_,
                            int num_threads_, int32_t bitlength_, int32_t kScale_,
                            std::istream &in = std::cin);

void run_densenet121_inference(int party_, int port_, const std::string &address_,
                               int num_threads_, int32_t bitlength_, int32_t kScale_);

#endif  // NETWORKS_NETWORKS_H_
