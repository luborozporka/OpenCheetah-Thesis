#ifndef ORCHESTRATION_COMMON_CSV_LOG_H_
#define ORCHESTRATION_COMMON_CSV_LOG_H_

#include <fstream>
#include <string>
#include <vector>

namespace orchestration {

class CsvLog {
 public:
  CsvLog(const std::string &path, const std::vector<std::string> &headers);

  CsvLog(const CsvLog &) = delete;
  CsvLog &operator=(const CsvLog &) = delete;

  void WriteRow(const std::vector<std::string> &values);

 private:
  static std::string Escape(const std::string &value);
  void WriteValues(const std::vector<std::string> &values);

  std::ofstream out_;
};

}  // namespace orchestration

#endif  // ORCHESTRATION_COMMON_CSV_LOG_H_
