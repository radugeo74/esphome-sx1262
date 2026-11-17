#pragma once
// Experimental offline shadow comparison for already decoded source fixtures.
// No live ESPHome hooks, no writes to meter state, no logging of AES keys.
// The reference represents selected, explicitly known old/upstream outputs.
#include "xmq_frame_bridge.h"
#include <cmath>
#include <cstddef>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace xmq_runtime {

struct ShadowReference {
  std::map<std::string, double> numeric;
  std::map<std::string, std::string> dates;
  std::vector<std::string> absent_numeric;
  std::vector<std::string> absent_dates;
};

struct ShadowMismatch {
  std::string field;
  std::string reason;
};

struct ShadowReport {
  std::size_t compared = 0;
  std::vector<ShadowMismatch> mismatches;
  bool equal() const { return mismatches.empty(); }
};

inline ShadowReport compare_shadow_reference(
    const XmqFrameResult &actual, const ShadowReference &reference,
    double tolerance=1e-6) {
  if (!(tolerance>=0.0) || !std::isfinite(tolerance) ||
      reference.numeric.size()+reference.dates.size()+
      reference.absent_numeric.size()+reference.absent_dates.size()>128)
    throw std::invalid_argument("invalid shadow comparison limits");
  ShadowReport report;
  for (const auto &expected:reference.numeric) {
    ++report.compared;
    const auto hit=actual.numeric.find(expected.first);
    if (hit==actual.numeric.end()) {
      report.mismatches.push_back({expected.first,"missing numeric"});
    } else if (!std::isfinite(hit->second.value) ||
               !std::isfinite(expected.second) ||
               std::fabs(hit->second.value-expected.second)>tolerance) {
      report.mismatches.push_back({expected.first,"numeric differs"});
    }
  }
  for (const auto &expected:reference.dates) {
    ++report.compared;
    const auto hit=actual.dates.find(expected.first);
    if (hit==actual.dates.end())
      report.mismatches.push_back({expected.first,"missing date"});
    else if (hit->second!=expected.second)
      report.mismatches.push_back({expected.first,"date differs"});
  }
  for (const auto &name:reference.absent_numeric) {
    ++report.compared;
    if (actual.numeric.count(name))
      report.mismatches.push_back({name,"unexpected numeric"});
  }
  for (const auto &name:reference.absent_dates) {
    ++report.compared;
    if (actual.dates.count(name))
      report.mismatches.push_back({name,"unexpected date"});
  }
  return report;
}

} // namespace xmq_runtime
