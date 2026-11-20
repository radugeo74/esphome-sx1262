#include "xmq_legacy_write_report.h"
#include <iostream>
#include <stdexcept>

static void require(bool v, const char *why) {
  if (!v) throw std::runtime_error(why);
}

int main() {
  try {
    using namespace xmq_runtime;
    LegacyWriteJournal j(8, 8);
    j.begin_telegram();
    j.try_numeric_write("current_hca");
    j.try_numeric_write("previous_hca");
    j.try_numeric_write("temp_radiator_c");
    j.try_string_write("previous_date");
    auto h = summarize_legacy_writes(j, LegacyReportDriver::Fhkvdataiii);
    require(h.numeric_field_mask == 0b1011 && h.date_field_mask == 0b10,
            "heat field attribution wrong");
    require(h.numeric_writes == 3 && h.string_writes == 1 && h.dropped_writes == 0,
            "heat write counts wrong");
    const auto unknown = summarize_legacy_writes(j, LegacyReportDriver::Unknown);
    require(unknown.numeric_field_mask == 0 && unknown.date_field_mask == 0,
            "unsupported driver leaked attribution");
    j.end_telegram();
    j.begin_telegram();
    j.try_numeric_write("target_m3");
    j.try_numeric_write("target_m3"); // Same-value writes still observed.
    j.try_numeric_write("total_m3");
    auto w = summarize_legacy_writes(j, LegacyReportDriver::Mkradio4);
    require(w.numeric_field_mask == 0b11 && w.date_field_mask == 0 &&
            w.numeric_writes == 3, "water field attribution wrong");
    j.end_telegram();
    // Original upstream driver setters use unsuffixed internal field names.
    // Their observer masks must match the existing public-name fixtures.
    j.begin_telegram();
    j.try_numeric_write("target");
    j.try_numeric_write("total");
    auto upstream_water = summarize_legacy_writes(j, LegacyReportDriver::Mkradio4);
    require(upstream_water.numeric_field_mask == 0b11 &&
            upstream_water.numeric_writes == 2 &&
            upstream_water.dropped_writes == 0,
            "upstream water internal field names not attributed");
    j.end_telegram();
    j.begin_telegram();
    j.try_numeric_write("current");
    j.try_numeric_write("previous");
    j.try_numeric_write("temp_room");
    j.try_numeric_write("temp_radiator");
    j.try_string_write("current_date");
    auto upstream_heat = summarize_legacy_writes(j, LegacyReportDriver::Fhkvdataiii);
    require(upstream_heat.numeric_field_mask == 0b1111 &&
            upstream_heat.date_field_mask == 0b01 &&
            upstream_heat.numeric_writes == 4,
            "upstream heat internal field names not attributed");
    j.end_telegram();
    j.begin_telegram();
    j.try_numeric_write("total_m3");
    j.try_numeric_write("total");
    auto mixed = summarize_legacy_writes(j, LegacyReportDriver::Mkradio4);
    require(mixed.numeric_field_mask == 0b10 && mixed.numeric_writes == 2,
            "alias and upstream field doubled mask");
    j.end_telegram();
    j.begin_telegram();
    require(summarize_legacy_writes(j, LegacyReportDriver::Mkradio4)
              .numeric_field_mask == 0, "old telegram attribution leaked");
    std::cout << "LEGACY_WRITE_REPORT_OK: heat/water aliases and upstream names, unchanged-value writes, "
                 "no leak, unsupported driver excluded\n";
  } catch (const std::exception &e) {
    std::cerr << "FAIL: " << e.what() << '\n';
    return 1;
  }
}
