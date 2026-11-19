#pragma once
// Isolated observer dispatch. Only aggregate counts/field masks cross the API.
// Not connected to the live ESPHome decoder.
#include "xmq_legacy_write_report.h"

namespace xmq_runtime {
using LegacyWriteObserver = void (*)(LegacyWriteReport) noexcept;

// Dispatch runs after the setter collection scope. Fail-open by construction:
// if no observer is registered, the real decoder's behavior is unchanged.
inline bool dispatch_legacy_write_report(
    const LegacyWriteJournal &journal, LegacyReportDriver driver,
    LegacyWriteObserver observer) noexcept {
  if (observer == nullptr || driver == LegacyReportDriver::Unknown)
    return false;
  try {
    observer(summarize_legacy_writes(journal, driver));
    return true;
  } catch (...) {
    return false;
  }
}
}  // namespace xmq_runtime
