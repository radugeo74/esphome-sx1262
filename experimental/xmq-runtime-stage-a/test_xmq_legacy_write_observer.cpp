#include "xmq_legacy_write_observer.h"
#include <iostream>
#include <stdexcept>

static xmq_runtime::LegacyWriteReport observed{};
static unsigned callbacks = 0;
static void capture(xmq_runtime::LegacyWriteReport value) noexcept {
  observed = value;
  ++callbacks;
}
static void require(bool value, const char *reason) {
  if (!value) throw std::runtime_error(reason);
}

int main() {
  try {
    using namespace xmq_runtime;
    LegacyWriteJournal journal(6, 6);
    journal.begin_telegram();
    require(!dispatch_legacy_write_report(journal, LegacyReportDriver::Fhkvdataiii, nullptr),
            "unregistered callback must not run");
    require(!dispatch_legacy_write_report(journal, LegacyReportDriver::Unknown, capture),
            "unknown driver must not notify");
    journal.try_numeric_write("current_hca");
    journal.try_numeric_write("current_hca"); // Repeated writes may have same value.
    journal.try_numeric_write("previous_hca");
    journal.try_string_write("previous_date");
    journal.end_telegram();
    require(dispatch_legacy_write_report(journal, LegacyReportDriver::Fhkvdataiii, capture),
            "heat report was not dispatched");
    require(callbacks == 1 && observed.numeric_field_mask == 0b0011 &&
            observed.date_field_mask == 0b10 && observed.numeric_writes == 3 &&
            observed.string_writes == 1 && observed.dropped_writes == 0,
            "heat callback did not receive expected aggregate");
    journal.begin_telegram();
    journal.try_numeric_write("total_m3");
    journal.end_telegram();
    require(dispatch_legacy_write_report(journal, LegacyReportDriver::Mkradio4, capture),
            "water report was not dispatched");
    require(callbacks == 2 && observed.numeric_field_mask == 0b10 &&
            observed.date_field_mask == 0 && observed.numeric_writes == 1 &&
            observed.string_writes == 0,
            "prior telegram leaked into water callback");
    // Evidence must be per telegram, not reconstructed from cached values.
    require(legacy_write_evidence_complete(observed, LegacyReportDriver::Mkradio4,
                                            0b10),
            "observed water total write was not accepted");
    require(!legacy_write_evidence_complete(observed, LegacyReportDriver::Mkradio4,
                                             0b11),
            "missing water target falsely accepted");
    require(!legacy_write_evidence_complete(observed, LegacyReportDriver::Mkradio4,
                                             0),
            "zero-field evidence falsely accepted");
    require(!legacy_write_evidence_complete(observed, LegacyReportDriver::Mkradio4,
                                             0b100),
            "unknown water bit accepted");
    require(!legacy_write_evidence_complete(observed, LegacyReportDriver::Mkradio4,
                                             0b10, 0b01),
            "water date bit accepted");
    require(!legacy_write_evidence_complete(observed, LegacyReportDriver::Unknown,
                                             0b10),
            "unknown driver falsely accepted");
    LegacyWriteReport dropped = observed;
    dropped.dropped_writes = 1;
    require(!legacy_write_evidence_complete(dropped, LegacyReportDriver::Mkradio4,
                                             0b10),
            "incomplete write journal falsely accepted");
    LegacyWriteReport heat_evidence{};
    heat_evidence.numeric_field_mask = 0b0011;
    heat_evidence.date_field_mask = 0b10;
    require(legacy_write_evidence_complete(heat_evidence,
                                            LegacyReportDriver::Fhkvdataiii,
                                            0b0011, 0b10),
            "valid partial heat evidence rejected");
    require(!legacy_write_evidence_complete(heat_evidence,
                                             LegacyReportDriver::Fhkvdataiii,
                                             0b1111, 0b10),
            "missing heat fields falsely accepted");
    std::cout << "LEGACY_WRITE_OBSERVER_OK: opt-in callback executed, "
                 "per-frame summary isolated, values never passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL: " << e.what() << '\n';
    return 1;
  }
}
