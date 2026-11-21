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

// Strict YYYY-MM-DD calendar validity; formatting alone is insufficient for
// a sensor-ready date (e.g. 2025-99-99 must fail).
inline bool valid_primary_iso_date(const std::string &s) noexcept {
  if (s.size() != 10 || s[4] != '-' || s[7] != '-') return false;
  for (std::size_t i = 0; i < s.size(); ++i) {
    if (i == 4 || i == 7) continue;
    if (s[i] < '0' || s[i] > '9') return false;
  }
  const int year = (s[0]-'0')*1000 + (s[1]-'0')*100 +
                   (s[2]-'0')*10 + (s[3]-'0');
  const int month = (s[5]-'0')*10 + (s[6]-'0');
  const int day = (s[8]-'0')*10 + (s[9]-'0');
  if (year == 0 || month < 1 || month > 12 || day < 1) return false;
  const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
  const int days[] = {31, leap ? 29 : 28, 31, 30, 31, 30,
                      31, 31, 30, 31, 30, 31};
  return day <= days[month - 1];
}

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
      if (it != output.dates.end() && valid_primary_iso_date(it->second))
        ++report.available;
    }
    report.ready = report.available == report.required;
  } catch (...) {
    report.ready = false;
  }
  return report;
}
// Decode verified application bytes using XMQ ONLY, then gate the complete
// expected fields before returning them. Never reads legacy snapshots.
// On ANY failure the destination is cleared and is not publishable.
// This remains host-only until a reviewed on-device adapter exists.
inline bool decode_primary_candidate(
    const DriverRegistry &registry, const Mvt &inner_mvt,
    ApplicationEncoding encoding, const std::vector<uint8_t> &clear_application,
    const std::string &expected_driver,
    const std::vector<std::string> &required_numeric,
    const std::vector<std::string> &required_dates,
    XmqFrameResult &destination) noexcept {
  try {
    destination = XmqFrameResult{};
    // Do not accept auto-detection selecting a different driver.
    const auto projected = project_verified_application(
        registry, inner_mvt, encoding, clear_application, expected_driver);
    const auto ready = inspect_primary_readiness(
        projected, expected_driver, required_numeric, required_dates);
    if (!ready.ready) return false;
    destination = projected;
    return true;
  } catch (...) {
    destination = XmqFrameResult{};
    return false;
  }
}

} // namespace xmq_runtime
