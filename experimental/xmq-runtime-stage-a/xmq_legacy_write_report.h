#pragma once
// Aggregate-only, host-testable legacy setter write provenance.
// This API never includes meter IDs, payloads, AES keys or decoded values.
#include "xmq_legacy_write_journal.h"
#include <cstdint>

namespace xmq_runtime {

enum class LegacyReportDriver : std::uint8_t { Unknown, Fhkvdataiii, Mkradio4 };

struct LegacyWriteReport {
  std::uint32_t numeric_field_mask{0};
  std::uint32_t date_field_mask{0};
  std::uint32_t numeric_writes{0};
  std::uint32_t string_writes{0};
  std::uint32_t dropped_writes{0};
};

// Bits 0-3: current_hca, previous_hca, temp_room_c, temp_radiator_c.
// Bits 0-1 for water: target_m3, total_m3.
// Date bits 0-1 for heat: current_date, previous_date.
// No stored values are consulted: a same-value setter call remains observed.
inline LegacyWriteReport summarize_legacy_writes(
    const LegacyWriteJournal &journal, LegacyReportDriver driver) {
  LegacyWriteReport report{};
  const auto &counts = journal.counts();
  report.numeric_writes = static_cast<std::uint32_t>(counts.numeric_writes);
  report.string_writes = static_cast<std::uint32_t>(counts.string_writes);
  report.dropped_writes = static_cast<std::uint32_t>(journal.dropped_writes());
  // The real upstream drivers register their fields WITHOUT a unit suffix:
  // e.g. "total", "target", "current", "temp_room". ESPHome exposes the
  // corresponding named values with "_m3", "_hca" and "_c". Accept both
  // representations, but count one logical field only once.
  auto number = [&](const char *public_field, const char *upstream_field,
                    unsigned bit) {
    if (journal.wrote_numeric(public_field) ||
        journal.wrote_numeric(upstream_field))
      report.numeric_field_mask |= (std::uint32_t{1} << bit);
  };
  auto date = [&](const char *public_field, const char *upstream_field,
                  unsigned bit) {
    if (journal.wrote_string(public_field) ||
        journal.wrote_string(upstream_field))
      report.date_field_mask |= (std::uint32_t{1} << bit);
  };
  switch (driver) {
    case LegacyReportDriver::Fhkvdataiii:
      number("current_hca", "current", 0);
      number("previous_hca", "previous", 1);
      number("temp_room_c", "temp_room", 2);
      number("temp_radiator_c", "temp_radiator", 3);
      date("current_date", "current", 0);
      date("previous_date", "previous", 1);
      break;
    case LegacyReportDriver::Mkradio4:
      number("target_m3", "target", 0);
      number("total_m3", "total", 1);
      break;
    case LegacyReportDriver::Unknown:
      break;
  }
  return report;
}
// Host-only evidence gate: a field snapshot is not proof of a write.
// Callers supply the exact required masks for their telegram variant.
// Partial OMS frames may legitimately require a subset; never infer the masks
// from values cached on a meter. Zero-field evidence is always inconclusive.
inline bool legacy_write_evidence_complete(
    const LegacyWriteReport &report, LegacyReportDriver driver,
    std::uint32_t required_numeric_mask,
    std::uint32_t required_date_mask = 0) noexcept {
  if (driver == LegacyReportDriver::Unknown || report.dropped_writes != 0)
    return false;
  const std::uint32_t allowed_numeric =
      driver == LegacyReportDriver::Fhkvdataiii ? 0x0FU : 0x03U;
  const std::uint32_t allowed_dates =
      driver == LegacyReportDriver::Fhkvdataiii ? 0x03U : 0U;
  if ((required_numeric_mask | required_date_mask) == 0 ||
      (required_numeric_mask & ~allowed_numeric) != 0 ||
      (required_date_mask & ~allowed_dates) != 0)
    return false;
  // Every distinct reported field requires at least one completed setter
  // call. A manually inconsistent mask without matching write counts is not
  // evidence, even if the numeric snapshot happens to match XMQ.
  auto bit_count = [](std::uint32_t mask) noexcept {
    std::uint32_t count = 0;
    while (mask != 0) {
      count += mask & 1U;
      mask >>= 1U;
    }
    return count;
  };
  if (bit_count(report.numeric_field_mask) > report.numeric_writes ||
      bit_count(report.date_field_mask) > report.string_writes)
    return false;
  return (report.numeric_field_mask & required_numeric_mask) ==
             required_numeric_mask &&
         (report.date_field_mask & required_date_mask) == required_date_mask;
}

}  // namespace xmq_runtime
