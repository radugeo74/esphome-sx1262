#pragma once
// HOST/LAB ONLY. This bridge is not wired into the live ESPHome decoder.
// Prevent a legacy snapshot equality from being labelled as proven parity
// without a complete same-telegram setter report.
#include "xmq_legacy_write_report.h"
#include "xmq_shadow_diagnostic.h"

namespace xmq_runtime {

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
  if (!legacy_write_evidence_complete(writes, driver,
                                      required_numeric_mask,
                                      required_date_mask))
    return ShadowDisposition::IncompleteReference;
  return diagnostic.inspect(registry, inner_mvt, encoding, clear_application,
                            reference, 0, false, explicit_driver,
                            expected_numeric);
}
}  // namespace xmq_runtime
