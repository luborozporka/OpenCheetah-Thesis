#include "orchestration/knowledge_base.h"
#include "orchestration/common/protocol_internal.h"

#include <cstdlib>
#include <fstream>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace orchestration {
namespace {

using protocol_internal::ParseDouble;
using protocol_internal::ParseInt64;
using protocol_internal::ParseUint64;
using protocol_internal::Trim;

bool SplitCsvLine(const std::string &line,
                  std::vector<std::string> *fields,
                  std::string *error) {
  fields->clear();
  std::string field;
  bool in_quotes = false;

  for (size_t i = 0; i < line.size(); ++i) {
    const char ch = line[i];
    if (ch == '"') {
      if (in_quotes && i + 1 < line.size() && line[i + 1] == '"') {
        field += '"';
        ++i;
      } else {
        in_quotes = !in_quotes;
      }
    } else if (ch == ',' && !in_quotes) {
      fields->push_back(Trim(field));
      field.clear();
    } else {
      field += ch;
    }
  }

  if (in_quotes) {
    if (error != nullptr) *error = "unterminated quoted CSV field";
    return false;
  }

  fields->push_back(Trim(field));
  return true;
}

std::map<std::string, size_t> HeaderIndex(const std::vector<std::string> &headers) {
  std::map<std::string, size_t> result;
  for (size_t i = 0; i < headers.size(); ++i) {
    result[headers[i]] = i;
  }
  return result;
}

std::string CsvField(const std::vector<std::string> &row,
                     const std::map<std::string, size_t> &headers,
                     const std::string &name) {
  const auto it = headers.find(name);
  if (it == headers.end() || it->second >= row.size()) return "";
  return row[it->second];
}

bool ParseInt32Field(const std::vector<std::string> &row,
                     const std::map<std::string, size_t> &headers,
                     const std::string &name,
                     int32_t *out,
                     std::string *error) {
  const std::string value = CsvField(row, headers, name);
  if (value.empty()) return true;

  int64_t parsed = 0;
  if (!ParseInt64(value, &parsed)) {
    if (error != nullptr) *error = "invalid integer field: " + name;
    return false;
  }
  *out = static_cast<int32_t>(parsed);
  return true;
}

bool ParseInt64Field(const std::vector<std::string> &row,
                     const std::map<std::string, size_t> &headers,
                     const std::string &name,
                     int64_t *out,
                     std::string *error) {
  const std::string value = CsvField(row, headers, name);
  if (value.empty()) return true;

  if (!ParseInt64(value, out)) {
    if (error != nullptr) *error = "invalid integer field: " + name;
    return false;
  }
  return true;
}

bool ParseUint64Field(const std::vector<std::string> &row,
                      const std::map<std::string, size_t> &headers,
                      const std::string &name,
                      uint64_t *out,
                      std::string *error) {
  const std::string value = CsvField(row, headers, name);
  if (value.empty()) return true;

  if (!ParseUint64(value, out)) {
    if (error != nullptr) *error = "invalid unsigned integer field: " + name;
    return false;
  }
  return true;
}

bool ParseDoubleField(const std::vector<std::string> &row,
                      const std::map<std::string, size_t> &headers,
                      const std::string &name,
                      double *out,
                      std::string *error) {
  const std::string value = CsvField(row, headers, name);
  if (value.empty()) return true;

  if (!ParseDouble(value, out)) {
    if (error != nullptr) *error = "invalid floating-point field: " + name;
    return false;
  }
  return true;
}

bool ParseEntry(const std::vector<std::string> &row,
                const std::map<std::string, size_t> &headers,
                KnowledgeBaseEntry *entry,
                std::string *error) {
  KnowledgeBaseEntry parsed;
  parsed.node_id = CsvField(row, headers, "node_id");
  parsed.input_shape = CsvField(row, headers, "input_shape");

  const std::string node_role = CsvField(row, headers, "node_role");
  if (!node_role.empty() && !ParseNodeRole(node_role, &parsed.node_role)) {
    if (error != nullptr) *error = "invalid node_role";
    return false;
  }
  if (!ParseBackend(CsvField(row, headers, "backend"), &parsed.backend)) {
    if (error != nullptr) *error = "invalid backend";
    return false;
  }
  if (!ParseNetwork(CsvField(row, headers, "network"), &parsed.network)) {
    if (error != nullptr) *error = "invalid network";
    return false;
  }
  if (parsed.node_id.empty()) {
    if (error != nullptr) *error = "missing node_id";
    return false;
  }
  if (!ParseInt32Field(row, headers, "num_threads", &parsed.num_threads, error) ||
      !ParseInt32Field(row, headers, "concurrency_level", &parsed.concurrency_level, error) ||
      !ParseDoubleField(row, headers, "accuracy_top1", &parsed.accuracy_top1, error) ||
      !ParseDoubleField(row, headers, "mean_latency_ms", &parsed.mean_latency_ms, error) ||
      !ParseDoubleField(row, headers, "std_latency_ms", &parsed.std_latency_ms, error) ||
      !ParseDoubleField(row, headers, "mean_energy_j", &parsed.mean_energy_j, error) ||
      !ParseDoubleField(row, headers, "std_energy_j", &parsed.std_energy_j, error) ||
      !ParseUint64Field(row, headers, "samples", &parsed.samples, error) ||
      !ParseInt64Field(row, headers, "last_updated_ms", &parsed.last_updated_ms, error)) {
    return false;
  }

  *entry = parsed;
  return true;
}

bool InputShapeMatches(const KnowledgeBaseEntry &entry, const KnowledgeBaseQuery &query) {
  return query.input_shape.empty() || entry.input_shape == query.input_shape;
}

bool WorkloadMatches(const KnowledgeBaseEntry &entry, const KnowledgeBaseQuery &query) {
  return entry.node_role == NodeRole::kServer &&
         entry.backend == query.backend &&
         entry.network == query.network &&
         entry.num_threads == query.num_threads &&
         InputShapeMatches(entry, query);
}

bool ExactMatches(const KnowledgeBaseEntry &entry, const KnowledgeBaseQuery &query) {
  return WorkloadMatches(entry, query) &&
         entry.node_id == query.node_id &&
         entry.concurrency_level == query.concurrency_level;
}

KnowledgeBaseLookup MakeLookup(KnowledgeBaseLookupSource source, const KnowledgeBaseEntry &entry) {
  KnowledgeBaseLookup lookup;
  lookup.found = true;
  lookup.source = source;
  lookup.entry = entry;
  return lookup;
}

bool CloserLowerConcurrency(const KnowledgeBaseEntry &candidate,
                            const KnowledgeBaseEntry *best,
                            int32_t target_concurrency) {
  if (best == nullptr) return true;
  return candidate.concurrency_level > best->concurrency_level &&
         candidate.concurrency_level <= target_concurrency;
}

bool CloserAnyConcurrency(const KnowledgeBaseEntry &candidate,
                          const KnowledgeBaseEntry *best,
                          int32_t target_concurrency) {
  if (best == nullptr) return true;
  const int candidate_distance = std::abs(candidate.concurrency_level - target_concurrency);
  const int best_distance = std::abs(best->concurrency_level - target_concurrency);
  if (candidate_distance != best_distance) {
    return candidate_distance < best_distance;
  }
  if (candidate.node_id != best->node_id) return candidate.node_id < best->node_id;
  return candidate.concurrency_level < best->concurrency_level;
}

}  // namespace

std::string ToString(KnowledgeBaseLookupSource source) {
  switch (source) {
    case KnowledgeBaseLookupSource::kExact:
      return "exact";
    case KnowledgeBaseLookupSource::kSameNodeLowerConcurrency:
      return "same_node_lower_concurrency";
    case KnowledgeBaseLookupSource::kPeerSameWorkload:
      return "peer_same_workload";
    case KnowledgeBaseLookupSource::kSameWorkloadAnyConcurrency:
      return "same_workload_any_concurrency";
    case KnowledgeBaseLookupSource::kDefault:
      return "default";
    case KnowledgeBaseLookupSource::kUnknown:
      return "unknown";
  }
  return "unknown";
}

bool KnowledgeBase::LoadCsv(const std::string &path, std::string *error) {
  std::ifstream input(path);
  if (!input) {
    if (error != nullptr) *error = "failed to open knowledge base: " + path;
    return false;
  }

  std::string line;
  std::vector<std::string> fields;
  std::map<std::string, size_t> headers;
  std::vector<KnowledgeBaseEntry> loaded;
  size_t line_number = 0;
  bool have_header = false;

  while (std::getline(input, line)) {
    ++line_number;
    const std::string trimmed = Trim(line);
    if (trimmed.empty() || trimmed[0] == '#') continue;

    std::string parse_error;
    if (!SplitCsvLine(line, &fields, &parse_error)) {
      if (error != nullptr) {
        *error = "line " + std::to_string(line_number) + ": " + parse_error;
      }
      return false;
    }

    if (!have_header) {
      headers = HeaderIndex(fields);
      have_header = true;
      continue;
    }

    KnowledgeBaseEntry entry;
    if (!ParseEntry(fields, headers, &entry, &parse_error)) {
      if (error != nullptr) {
        *error = "line " + std::to_string(line_number) + ": " + parse_error;
      }
      return false;
    }
    loaded.push_back(entry);
  }

  entries_ = std::move(loaded);
  return true;
}

void KnowledgeBase::AddEntry(const KnowledgeBaseEntry &entry) {
  entries_.push_back(entry);
}

KnowledgeBaseLookup KnowledgeBase::Lookup(const KnowledgeBaseQuery &query) const {
  for (const auto &entry : entries_) {
    if (ExactMatches(entry, query)) {
      return MakeLookup(KnowledgeBaseLookupSource::kExact, entry);
    }
  }

  const KnowledgeBaseEntry *best = nullptr;
  for (const auto &entry : entries_) {
    if (!WorkloadMatches(entry, query)) continue;
    if (entry.node_id != query.node_id) continue;
    if (entry.concurrency_level > query.concurrency_level) continue;
    if (CloserLowerConcurrency(entry, best, query.concurrency_level)) {
      best = &entry;
    }
  }
  if (best != nullptr) {
    return MakeLookup(KnowledgeBaseLookupSource::kSameNodeLowerConcurrency, *best);
  }

  best = nullptr;
  for (const auto &entry : entries_) {
    if (!WorkloadMatches(entry, query)) continue;
    if (entry.concurrency_level != query.concurrency_level) continue;
    if (best == nullptr || entry.node_id < best->node_id) {
      best = &entry;
    }
  }
  if (best != nullptr) {
    return MakeLookup(KnowledgeBaseLookupSource::kPeerSameWorkload, *best);
  }

  best = nullptr;
  for (const auto &entry : entries_) {
    if (!WorkloadMatches(entry, query)) continue;
    if (CloserAnyConcurrency(entry, best, query.concurrency_level)) {
      best = &entry;
    }
  }
  if (best != nullptr) {
    return MakeLookup(KnowledgeBaseLookupSource::kSameWorkloadAnyConcurrency, *best);
  }

  KnowledgeBaseLookup lookup;
  lookup.source = KnowledgeBaseLookupSource::kDefault;
  return lookup;
}

}  // namespace orchestration
