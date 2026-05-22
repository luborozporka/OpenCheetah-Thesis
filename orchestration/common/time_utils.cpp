#include "orchestration/common/time_utils.h"

#include <atomic>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace orchestration {

int64_t NowMillis() {
  const auto now = std::chrono::system_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

std::string TimestampForPath() {
  const std::time_t now = std::time(nullptr);
  std::tm local_time{};
  localtime_r(&now, &local_time);

  std::ostringstream out;
  out << std::put_time(&local_time, "%Y-%m-%d_%H-%M-%S");
  return out.str();
}

std::string MakeRequestId() {
  static std::atomic<uint64_t> counter{0};
  return std::to_string(NowMillis()) + "-" + std::to_string(counter.fetch_add(1));
}

}
