#include "orchestration/common/time_utils.h"

#include <chrono>

namespace orchestration {

int64_t NowMillis() {
  const auto now = std::chrono::system_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

}
