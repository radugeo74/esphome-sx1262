#pragma once
// Host-only primary-decoder readiness gate. This checks the XMQ-produced
// values themselves: NO legacy snapshot, sensor publishing, radio or keys.
// Caller must provide verified/decrypted application bytes and inner MVT.
#include "xmq_frame_bridge.h"
#include <algorithm>
#include <cstddef>
#include <cmath>
#include <string>
#include <vector>

namespace xmq_runtime {

struct PrimaryReadiness {
  bool ready{false};
  std::size_t required{0};
  std::size_t available{0};
};

inline PrimaryReadiness inspect_primary_readiness(
    const XmqFrameResult &output, const std::string &expected_driver,
    const std::vector<std::string> &required_numeric,
    const std::vector<std::string> &required_dates = {}) noexcept {
  PrimaryReadiness report{};
  report.required = required_numeric.size() + required_dates.size();
  if (expected_driver.empty() || output.driver != expected_driver ||
      report.required == 0 || report.required > 16)
    return report;
  try {
    // Duplicates and overlapping numeric/date names make a contract invalid.
    std::vector<std::string> seen;
    for (const auto &field : required_numeric) {
      if (field.empty() || field.size() > 96)
        return report;
      if (std::find(seen.begin(), seen.end(), field) != seen.end())
        return report;
      seen.push_back(field);
      const auto it = output.numeric.find(field);
      if (it != output.numeric.end() && std::isfinite(it->second.value))
        ++report.available;
    }
    for (const auto &field : required_dates) {
      if (field.empty() || field.size() > 96)
        return report;
      if (std::find(seen.begin(), seen.end(), field) != seen.end())
        return report;
      seen.push_back(field);
      const auto it = output.dates.find(field);
      if (it != output.dates.end() && it->second.size() == 10 &&
          it->second[4] == '-' && it->second[7] == '-')
        ++report.available;
    }
    report.ready = report.available == report.required;
  } catch (...) {
    report.ready = false;
  }
  return report;
}
} // namespace xmq_runtime
