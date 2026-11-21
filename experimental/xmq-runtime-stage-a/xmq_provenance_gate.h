#pragma once
// HOST/LAB ONLY. This bridge is not wired into the live ESPHome decoder.
// Prevent a legacy snapshot equality from being labelled as proven parity
// without a complete same-telegram setter report.
#include "xmq_legacy_write_report.h"
#include "xmq_shadow_diagnostic.h"

namespace xmq_runtime {

// The caller-provided masks must describe EXACTLY the fields in the
// reference for this telegram; a partial mask must not bless cached fields.
// No allocations, no values copied, and unknown field names fail closed.
inline bool reference_matches_write_contract(
    const ShadowReference &reference, LegacyReportDriver driver,
    uint32_t required_numeric_mask, uint32_t required_date_mask) noexcept {
  if (driver == LegacyReportDriver::Unknown) return false;
  uint32_t actual_numeric = 0;
  uint32_t actual_dates = 0;
  for (const auto &entry : reference.numeric) {
    uint32_t bit = 0;
    const auto &name = entry.first;
    if (driver == LegacyReportDriver::Mkradio4) {
      if (name == "target_m3") bit = 1U;
      else if (name == "total_m3") bit = 2U;
    } else if (driver == LegacyReportDriver::Fhkvdataiii) {
      if (name == "current_hca") bit = 1U;
      else if (name == "previous_hca") bit = 2U;
      else if (name == "temp_room_c") bit = 4U;
      else if (name == "temp_radiator_c") bit = 8U;
    }
    if (bit == 0) return false;
    actual_numeric |= bit;
  }
  for (const auto &entry : reference.dates) {
    if (driver != LegacyReportDriver::Fhkvdataiii) return false;
    if (entry.first == "current_date") actual_dates |= 1U;
    else if (entry.first == "previous_date") actual_dates |= 2U;
    else return false;
  }
  // Absence tests are useful, but cannot substitute for positive
  // same-frame setter evidence. Require at least one present field.
  return (actual_numeric | actual_dates) != 0 &&
         actual_numeric == required_numeric_mask &&
         actual_dates == required_date_mask;
}

inline ShadowDisposition inspect_with_legacy_write_evidence(
    ShadowDiagnostic &diagnostic, const DriverRegistry &registry,
    const Mvt &inner_mvt, ApplicationEncoding encoding,
    const std::vector<uint8_t> &clear_application,
    const ShadowReference &reference,
    const LegacyWriteReport &writes, LegacyReportDriver driver,
    uint32_t required_numeric_mask, uint32_t required_date_mask = 0,
    const std::string &explicit_driver = "",
    const std::vector<std::string> &expected_numeric = {}) noexcept {
  // Even when a caller provides a complete cache, missing actual writes
  // invalidate that frame's parity evidence. No XMQ parser is invoked.
  if (!reference_matches_write_contract(reference, driver,
                                        required_numeric_mask,
                                        required_date_mask) ||
      !legacy_write_evidence_complete(writes, driver,
                                      required_numeric_mask,
                                      required_date_mask))
    return ShadowDisposition::IncompleteReference;
  return diagnostic.inspect(registry, inner_mvt, encoding, clear_application,
                            reference, 0, false, explicit_driver,
                            expected_numeric);
}
}  // namespace xmq_runtime
