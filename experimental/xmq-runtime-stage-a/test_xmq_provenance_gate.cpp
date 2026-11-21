#include "xmq_provenance_gate.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

static void require(bool ok, const char *msg) {
  if (!ok) throw std::runtime_error(msg);
}
int main() {
  using namespace xmq_runtime;
  try {
    std::ifstream f("experimental/xmq-runtime-stage-a/upstream/src/mkradio4.xmq");
    require(bool(f), "upstream fixture missing");
    const std::string source((std::istreambuf_iterator<char>(f)),
                              std::istreambuf_iterator<char>());
    DriverRegistry registry({load_driver(source)});
    const auto clear = parse_hex(
        "06702901006017030004000300000000000000000000000000000000000000000000");
    ShadowReference reference;
    reference.numeric = {{"target_m3", 0.1}, {"total_m3", 0.4}};
    ShadowDiagnostic diagnostic(4, 0.0001);
    diagnostic.set_enabled(true);
    LegacyWriteReport writes{};
    writes.numeric_field_mask = 0b10; // only total setter called
    writes.numeric_writes = 1;
    const Mvt meter{"TCH", 0x95, 0x62};
    const auto inspect = [&](const LegacyWriteReport &report) {
      return inspect_with_legacy_write_evidence(
          diagnostic, registry, meter, ApplicationEncoding::OriginalIxml,
          clear, reference, report, LegacyReportDriver::Mkradio4,
          0b11, 0, "mkradio4", {"target_m3", "total_m3"});
    };
    require(inspect(writes) == ShadowDisposition::IncompleteReference,
            "cached target incorrectly counted as same-frame write");
    require(diagnostic.counters().observations == 0,
            "unproven writes must not consume comparison budget");
    writes.numeric_field_mask = 0b11;
    writes.numeric_writes = 2;
    require(inspect(writes) == ShadowDisposition::Match,
            "complete per-frame writes and original XMQ values must match");
    require(diagnostic.counters().matched == 1 &&
            diagnostic.counters().last_compared == 2,
            "verified evidence comparison counters incorrect");
    writes.dropped_writes = 1;
    require(inspect(writes) == ShadowDisposition::IncompleteReference,
            "dropped journal entry falsely accepted");
    require(diagnostic.counters().observations == 1,
            "dropped report called XMQ parser");
    writes.dropped_writes = 0;
    reference.numeric["total_m3"] = 0.5;
    require(inspect(writes) == ShadowDisposition::Difference,
            "a real numeric mismatch was hidden by write evidence");
    require(diagnostic.counters().different == 1,
            "difference counter not recorded");
    std::cout << "XMQ_PROVENANCE_GATE_OK: same-frame write mask required, "
                 "dropped writes rejected, XMQ mismatch detected; HOST ONLY\\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL: " << e.what() << '\\n';
    return 1;
  }
}
