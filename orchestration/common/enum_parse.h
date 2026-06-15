#ifndef ORCHESTRATION_COMMON_ENUM_PARSE_H_
#define ORCHESTRATION_COMMON_ENUM_PARSE_H_

#include "orchestration/common/protocol_internal.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace orchestration::protocol_internal {

template <typename T, size_t N>
bool ParseEnum(const std::string &value,
               const std::array<std::pair<std::string_view, T>, N> &entries,
               T unknown_value,
               T *out) {
  const std::string normalized = Lower(Trim(value));
  for (const auto &entry : entries) {
    if (normalized == entry.first) {
      *out = entry.second;
      return true;
    }
  }

  *out = unknown_value;
  return false;
}

}

#endif
