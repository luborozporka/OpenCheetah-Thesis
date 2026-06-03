#ifndef ORCHESTRATION_KNOWLEDGE_BASE_H_
#define ORCHESTRATION_KNOWLEDGE_BASE_H_

#include "orchestration/common/protocol.h"

#include <cstdint>
#include <string>
#include <vector>

namespace orchestration {

enum class KnowledgeBaseLookupSource {
  kUnknown = 0,
  kExact,
  kSameNodeLowerConcurrency,
  kPeerSameWorkload,
  kSameWorkloadAnyConcurrency,
  kDefault,
};

std::string ToString(KnowledgeBaseLookupSource source);

struct KnowledgeBaseEntry {
  std::string node_id;
  NodeRole node_role = NodeRole::kServer;
  Backend backend = Backend::kUnknown;
  Network network = Network::kUnknown;
  std::string input_shape;
  int32_t num_threads = 0;
  int32_t concurrency_level = 0;
  double accuracy_top1 = -1.0;
  double mean_latency_ms = 0.0;
  double std_latency_ms = 0.0;
  double mean_energy_j = 0.0;
  double std_energy_j = 0.0;
  uint64_t samples = 0;
  int64_t last_updated_ms = 0;
};

struct KnowledgeBaseQuery {
  std::string node_id;
  Backend backend = Backend::kUnknown;
  Network network = Network::kUnknown;
  std::string input_shape;
  int32_t num_threads = 0;
  int32_t concurrency_level = 0;
};

struct KnowledgeBaseLookup {
  bool found = false;
  KnowledgeBaseLookupSource source = KnowledgeBaseLookupSource::kDefault;
  KnowledgeBaseEntry entry;
};

class KnowledgeBase {
 public:
  bool LoadCsv(const std::string &path, std::string *error);
  void AddEntry(const KnowledgeBaseEntry &entry);

  KnowledgeBaseLookup Lookup(const KnowledgeBaseQuery &query) const;
  const std::vector<KnowledgeBaseEntry> &entries() const { return entries_; }

 private:
  std::vector<KnowledgeBaseEntry> entries_;
};

}  // namespace orchestration

#endif  // ORCHESTRATION_KNOWLEDGE_BASE_H_
