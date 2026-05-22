#ifndef ORCHESTRATION_COMMON_PROTOCOL_INTERNAL_H_
#define ORCHESTRATION_COMMON_PROTOCOL_INTERNAL_H_

#include "orchestration/common/protocol.h"

#include <cstdint>
#include <string>

namespace orchestration::protocol_internal {

std::string Trim(const std::string &value);
std::string Lower(std::string value);

bool ParseBool(const std::string &value, bool *out);
bool ParseInt64(const std::string &value, int64_t *out);
bool ParseUint64(const std::string &value, uint64_t *out);
bool ParseDouble(const std::string &value, double *out);

bool HasField(const KeyValueMessage &fields, const std::string &key);
const std::string &GetField(const KeyValueMessage &fields, const std::string &key);
bool RequireField(const KeyValueMessage &fields, const std::string &key, std::string *error);

void AddIfNotEmpty(KeyValueMessage *fields, const std::string &key, const std::string &value);

}

#endif
