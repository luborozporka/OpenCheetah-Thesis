#include "orchestration/common/csv_log.h"

#include <stdexcept>

namespace orchestration {

CsvLog::CsvLog(const std::string &path,
               const std::vector<std::string> &headers)
    : out_(path, std::ios::out | std::ios::app) {
  if (!out_) {
    throw std::runtime_error("failed to open CSV log: " + path);
  }

  const std::streampos pos = out_.tellp();
  if (pos == 0) {
    WriteValues(headers);
  }
}

void CsvLog::WriteRow(const std::vector<std::string> &values) {
  WriteValues(values);
}

bool CsvLog::good() const { return out_.good(); }

std::string CsvLog::Escape(const std::string &value) {
  bool needs_quotes = false;
  for (char ch : value) {
    if (ch == ',' || ch == '"' || ch == '\n' || ch == '\r') {
      needs_quotes = true;
      break;
    }
  }
  if (!needs_quotes) return value;

  std::string escaped = "\"";
  for (char ch : value) {
    if (ch == '"') escaped += '"';
    escaped += ch;
  }
  escaped += '"';
  return escaped;
}

void CsvLog::WriteValues(const std::vector<std::string> &values) {
  for (size_t i = 0; i < values.size(); ++i) {
    if (i > 0) out_ << ',';
    out_ << Escape(values[i]);
  }
  out_ << '\n';
  out_.flush();
}

}  // namespace orchestration
