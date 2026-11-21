#include "xmq_provenance_gate.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

static void require(bool ok, const char *reason) {
  if (!ok) throw std::runtime_error(reason);
}

int main() {
  using namespace xmq_runtime;
  try {
    std::ifstream upstream(
        "experimental/xmq-runtime-stage-a/upstream/src/fhkvdataiii.xmq");
    require(bool(upstream), "original fhkvdataiii XMQ fixture missing");
    const std::string source((std::istreambuf_iterator<char>(upstream)),
                             std::istreambuf_iterator<char>());
    DriverRegistry registry({load_driver(source)});
    // Source-derived manufacturer clear application, version marker 0F.
    // This is host-only plaintext fixture data, not a private meter capture.
    const auto clear = parse_hex(
        "0F9F292D005024040011BD08380904000000070000000000000000000000000001000000000003140E");
    const Mvt meter{"TCH", 0x94, 0x80};
    ShadowReference reference;
    reference.numeric = {{"current_hca", 4}, {"previous_hca", 45},
                         {"temp_room_c", 22.37},
                         {"temp_radiator_c", 23.60}};
    reference.dates = {{"current_date", "2021-02-05"},
                       {"previous_date", "2020-12-31"}};
    ShadowDiagnostic diagnostic(8, 0.001);
    diagnostic.set_enabled(true);
    LegacyWriteReport writes{};
    writes.numeric_field_mask = 0x0f;
    writes.numeric_writes = 4;
    writes.date_field_mask = 0x01; // stale previous_date must NOT qualify
    writes.string_writes = 1;
    const auto inspect = [&](const LegacyWriteReport &report) {
      return inspect_with_legacy_write_evidence(
          diagnostic, registry, meter, ApplicationEncoding::OriginalIxml,
          clear, reference, report, LegacyReportDriver::Fhkvdataiii,
          0x0f, 0x03, "fhkvdataiii",
          {"current_hca", "previous_hca", "temp_room_c", "temp_radiator_c"});
    };
    require(inspect(writes) == ShadowDisposition::IncompleteReference,
            "stale previous_date falsely accepted as per-frame evidence");
    require(diagnostic.counters().observations == 0,
            "incomplete calendar setter proof consumed a shadow slot");

    writes.date_field_mask = 0x03;
    writes.string_writes = 2;
    require(inspect(writes) == ShadowDisposition::Match,
            "original fhkvdataiii XMQ fixture and complete setter evidence differ");
    require(diagnostic.counters().matched == 1 &&
            diagnostic.counters().last_compared == 6,
            "HCA/temperature/calendar comparison must cover six fields");

    writes.dropped_writes = 1;
    require(inspect(writes) == ShadowDisposition::IncompleteReference,
            "dropped heat write must invalidate proven reference");
    require(diagnostic.counters().observations == 1,
            "dropped write unexpectedly called XMQ evaluator");
    writes.dropped_writes = 0;
    // A stale cached value cannot be approved by a mask for other fields.
    reference.dates.erase("previous_date");
    require(inspect(writes) == ShadowDisposition::IncompleteReference,
            "missing previous_date falsely accepted by write mask");
    reference.dates["previous_date"] = "2020-12-31";
    reference.numeric["unsupported_heat_field"] = 1.0;
    require(inspect(writes) == ShadowDisposition::IncompleteReference,
            "unknown heat reference field was accepted");
    reference.numeric.erase("unsupported_heat_field");
    require(diagnostic.counters().observations == 1,
            "invalid heat field contract consumed diagnostic slot");

    reference.absent_dates.push_back("current_date");
    require(inspect(writes) == ShadowDisposition::IncompleteReference,
            "unproven absence of calendar setter was accepted");
    reference.absent_dates.clear();
    require(diagnostic.counters().observations == 1,
            "unproven absent date consumed XMQ comparison budget");

    reference.dates["previous_date"] = "2020-12-30";
    require(inspect(writes) == ShadowDisposition::Difference,
            "previous date mismatch hidden by complete write mask");
    require(diagnostic.counters().different == 1 &&
            diagnostic.counters().last_mismatches == 1,
            "calendar difference was not accounted");
    std::cout << "XMQ_HEAT_PROVENANCE_GATE_OK: six fields, same-frame "
                 "HCA/temperature/date setter masks and changed calendar; HOST ONLY\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
