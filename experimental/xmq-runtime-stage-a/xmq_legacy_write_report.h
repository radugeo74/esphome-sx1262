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
  auto number = [&](const char *field, unsigned bit) {
    if (journal.wrote_numeric(field))
      report.numeric_field_mask |= (std::uint32_t{1} << bit);
  };
  auto date = [&](const char *field, unsigned bit) {
    if (journal.wrote_string(field))
      report.date_field_mask |= (std::uint32_t{1} << bit);
  };
  switch (driver) {
    case LegacyReportDriver::Fhkvdataiii:
      number("current_hca", 0);
      number("previous_hca", 1);
      number("temp_room_c", 2);
      number("temp_radiator_c", 3);
      date("current_date", 0);
      date("previous_date", 1);
      break;
    case LegacyReportDriver::Mkradio4:
      number("target_m3", 0);
      number("total_m3", 1);
      break;
    case LegacyReportDriver::Unknown:
      break;
  }
  return report;
}
}  // namespace xmq_runtime
