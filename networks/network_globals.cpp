#include <cstdint>
#include <string>

thread_local int party = 0;
thread_local int port = 32000;
thread_local std::string address = "127.0.0.1";
thread_local int num_threads = 4;
thread_local int32_t bitlength = 32;
thread_local int32_t kScale = 12;
