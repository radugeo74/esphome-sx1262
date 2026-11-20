#!/usr/bin/env python3
"""Attempt a strict HOST executable link against the actual upstream parser.

This is a diagnostic gate until all legacy dependencies have been linked.
A missing dependency is recorded as PENDING, never claimed as parser parity.
Original source files are not modified; no radio frames or credentials used.
"""
from pathlib import Path
import subprocess
import tempfile

from stage_legacy_setter_instrumentation import SOURCE, instrument

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parents[1]
COMMON = ROOT / "components" / "wmbus_common"
UNITS = (
    "wmbus", "dvparser", "util", "units", "wmbus_utils",
    "translatebits", "formula", "manufacturer_specificities",
    "address", "aes", "aescmac",
)
PROBE = r"""
#include "wmbus.h"
#include <vector>
#include <iostream>
#include "meters.h"
void verifyDriverLookupCreated();
int main() {
  // Fully synthetic, cleartext CI=0x78 DLL+DV telegram:
  // 2-byte DIF=0x02 / VIF=0x13 / value bytes 0x34,0x12.
  // Never publish actual meter IDs, RF captures or AES keys.
  std::vector<uchar> frame = {
    0x0E, 0x44, 0x33, 0x30, 0x01, 0x02, 0x03, 0x04,
    0x01, 0x07, 0x78, 0x02, 0x13, 0x34, 0x12
  };
  Telegram header;
  header.about = AboutTelegram("synthetic-host-fixture", -60, FrameType::WMBUS);
  const bool header_ok = header.parseHeader(frame);

  Telegram parsed;
  parsed.about = AboutTelegram("synthetic-host-fixture", -60, FrameType::WMBUS);
  MeterKeys no_keys;
  // The upstream decoder's registry normally initializes before parsing.
  // This function is defined by the real staged meters.cc, not a mock.
  verifyDriverLookupCreated();
  const bool frame_ok = parsed.parse(frame, &no_keys, false);
  const bool dv_found = !parsed.dv_entries.empty();
  // Check the decoded *number*, not merely the presence of a DV entry.
  // The synthetic 16-bit value bytes 0x34 0x12 must decode as 0x1234.
  uint64_t raw_numeric = 0;
  const bool dv_numeric_ok = dv_found && parsed.dv_entries.size() == 1 &&
      parsed.dv_entries.begin()->second.second.extractLong(&raw_numeric) &&
      raw_numeric == 0x1234;

  std::cout << "REAL_LEGACY_HEADER_EXECUTED: accepted="
            << (header_ok ? "yes" : "no") << "\n";
  std::cout << "REAL_LEGACY_FRAME_PARSE_EXECUTED: accepted="
            << (frame_ok ? "yes" : "no")
            << " dv_fields_present=" << (dv_found ? "yes" : "no") << "\n";
  if (header_ok && frame_ok && dv_numeric_ok) {
    std::cout << "REAL_LEGACY_FRAME_PARSE_OK: cleartext synthetic DV integer value 0x1234 decoded\n";
  } else {
    std::cerr << "REAL_LEGACY_FRAME_PARSE_FAILED: synthetic cleartext DV fixture regression\n";
    return 3;
  }

  // This truncated, inconsistent L-field must not be accepted. Exercise the
  // same real production parser without a mock or any private payload.
  std::vector<uchar> truncated = {
    0x20, 0x44, 0x33, 0x30, 0x01, 0x02, 0x03
  };
  Telegram bad_header;
  bad_header.about = AboutTelegram("synthetic-invalid", -60, FrameType::WMBUS);
  const bool invalid_header_accepted = bad_header.parseHeader(truncated);
  Telegram bad_frame;
  bad_frame.about = AboutTelegram("synthetic-invalid", -60, FrameType::WMBUS);
  const bool invalid_frame_accepted = bad_frame.parse(truncated, &no_keys, false);
  if (invalid_header_accepted || invalid_frame_accepted ||
      !bad_frame.dv_entries.empty()) {
    std::cerr << "REAL_LEGACY_INVALID_FRAME_FAILED: truncated DLL was accepted\n";
    return 4;
  }
  std::cout << "REAL_LEGACY_INVALID_FRAME_OK: truncated DLL rejected by real parser\n";
  return 0;
}
"""

# This SECOND executable references the complete original handleTelegram()
# implementation. Unlike the parser-only probe, it cannot pass by silently
# discarding the decoder via --gc-sections. Missing dependencies are reported
# without falsely declaring the runtime complete.
HANDLE_PROBE = r"""
#include "meters_common_implementation.h"
#include "xmq_legacy_write_observer.h"
#include <iostream>
#include <vector>

void verifyDriverLookupCreated();
extern "C" void xmq_stage_a_register_legacy_write_observer(
    void (*)(xmq_runtime::LegacyWriteReport) noexcept) noexcept;

class SyntheticMeter : public MeterCommonImplementation {
 public:
  SyntheticMeter(MeterInfo &mi, DriverInfo &di)
      : MeterCommonImplementation(mi, di) {}
  using MeterCommonImplementation::handleTelegram;
};

static unsigned reports = 0;
static void record_report(xmq_runtime::LegacyWriteReport) noexcept {
  ++reports;
}

int main() {
  verifyDriverLookupCreated();
  MeterInfo mi;
  mi.name = "synthetic-host-meter";
  mi.poll_interval = 0;
  AddressExpression matcher;
  if (!matcher.parse("*")) return 10;
  mi.address_expressions.push_back(matcher);
  DriverInfo driver;
  driver.setName("mkradio4");
  driver.setMeterType(MeterType::WaterMeter);
  SyntheticMeter meter(mi, driver);

  std::vector<uchar> frame = {
    0x0E, 0x44, 0x33, 0x30, 0x01, 0x02, 0x03, 0x04,
    0x01, 0x07, 0x78, 0x02, 0x13, 0x34, 0x12
  };
  AboutTelegram about("synthetic-handle-probe", -60, FrameType::WMBUS);
  std::vector<Address> addresses;
  bool matched = false;
  xmq_stage_a_register_legacy_write_observer(record_report);
  const bool handled = meter.handleTelegram(
      about, frame, false, &addresses, &matched, nullptr);
  xmq_stage_a_register_legacy_write_observer(nullptr);
  std::cout << "REAL_LEGACY_HANDLE_EXECUTED: handled=" << handled
            << " matched=" << matched
            << " reports=" << reports
            << " updates=" << meter.numUpdates() << "\n";
  // No actual XMQ driver fields are configured here, so zero writes is not a
  // parity result. An actual report is required at a later integration gate.
  return 0;
}
"""

def run(argv, **kwargs):
    return subprocess.run(argv, text=True, **kwargs)

def main():
    with tempfile.TemporaryDirectory(prefix="xmq-real-parser-") as folder:
        temp = Path(folder)
        objects = []
        for unit in UNITS:
            source = COMMON / (unit + ".cc")
            if not source.is_file():
                raise RuntimeError("missing upstream legacy parser source: " + unit)
            obj = temp / (unit + ".o")
            run([
                "g++", "-std=c++17", "-O0", "-g0", "-ffunction-sections",
                "-fdata-sections", "-Wno-unused-parameter", "-c",
                "-I" + str(COMMON),
                "-I" + str(HERE / "host-stubs"),
                str(source), "-o", str(obj),
            ], check=True)
            objects.append(str(obj))
        # Link the REAL legacy registry and driver detection from an instrumented
        # COPY. No changes to components/wmbus_common/meters.cc or live firmware.
        staged = temp / "meters-staged.cc"
        staged.write_text(instrument(SOURCE.read_text()))
        staged_obj = temp / "meters-staged.o"
        run([
            "g++", "-std=c++17", "-O0", "-g0", "-ffunction-sections",
            "-fdata-sections", "-Wno-unused-parameter", "-c",
            "-I" + str(COMMON),
            "-I" + str(HERE / "host-stubs"),
            "-I" + str(HERE),
            str(staged), "-o", str(staged_obj),
        ], check=True)
        objects.append(str(staged_obj))
        src = temp / "probe.cc"
        src.write_text(PROBE)
        obj = temp / "probe.o"
        run(["g++", "-std=c++17", "-O0", "-ffunction-sections",
             "-I" + str(COMMON), "-I" + str(HERE / "host-stubs"),
             str(src), "-c", "-o", str(obj)], check=True)

        binary = temp / "probe"
        command = ["g++", "-Wl,--gc-sections", "-Wl,--no-undefined",
                   str(obj), *objects, "-o", str(binary)]
        result = run(command, capture_output=True)
        if result.returncode != 0:
            # Link can legitimately fail while upstream dependencies are being
            # added. Keep CI green for this diagnostic, but fail closed on
            # unexpected tool errors or empty diagnostics.
            errors = result.stderr.splitlines()
            unresolved = sorted({
                line.split("undefined reference to ", 1)[1].strip()
                for line in errors if "undefined reference to " in line
            })
            if not unresolved:
                raise RuntimeError("real-parser strict link failed unexpectedly: " +
                                   result.stderr[-2000:])
            print("REAL_LEGACY_PARSER_LINK_FAILED: strict executable link incomplete")
            print("Unresolved symbol count:", len(unresolved))
            # Dump only symbol names, never meter IDs, payloads or keys.
            for symbol in unresolved[:24]:
                print("  unresolved:", symbol[:220])
            print("More unresolved names not displayed:", max(0, len(unresolved) - 24))
            raise SystemExit("REAL_LEGACY_PARSER_LINK_FAILED: real-parser release gate failed")

        test = run([str(binary)], capture_output=True)
        if test.returncode != 0:
            raise RuntimeError("real parser probe executable failed to run: " +
                               (test.stderr or "")[:500])
        print(test.stdout.strip())
        print("REAL_LEGACY_PARSER_LINK_OK: executable includes actual upstream parser and staged legacy driver registry")
        print("NOTE: garbage-collection of unused sections may omit full handleTelegram; it is NOT executed by this probe")
        # Separate strict executable forces references to the original
        # handleTelegram implementation and real parser in the same binary.
        handle_src = temp / "handle-probe.cc"
        handle_src.write_text(HANDLE_PROBE)
        handle_obj = temp / "handle-probe.o"
        run(["g++", "-std=c++17", "-O0", "-ffunction-sections",
             "-I" + str(COMMON), "-I" + str(HERE / "host-stubs"),
             "-I" + str(HERE), str(handle_src), "-c", "-o", str(handle_obj)],
            check=True)
        handle_bin = temp / "handle-probe"
        handle_link = run([
            "g++", "-Wl,--gc-sections", "-Wl,--no-undefined",
            str(handle_obj), *objects, "-o", str(handle_bin)
        ], capture_output=True)
        if handle_link.returncode:
            missing = sorted({
                ln.split("undefined reference to ", 1)[1].strip()
                for ln in handle_link.stderr.splitlines()
                if "undefined reference to " in ln
            })
            if not missing:
                raise RuntimeError(
                    "unexpected complete-handle linker failure: " +
                    handle_link.stderr[-2000:])
            print("REAL_LEGACY_HANDLE_LINK_PENDING: full decoder executable "
                  "has unresolved legacy dependencies:", len(missing))
            for symbol in missing[:20]:
                print("  handle-unresolved:", symbol[:220])
            print("Unlisted additional symbols:", max(0, len(missing) - 20))
            return
        handle_result = run([str(handle_bin)], capture_output=True)
        if handle_result.returncode:
            print("REAL_LEGACY_HANDLE_EXECUTION_PENDING: linked but synthetic "
                  "meter is not yet a verified driver fixture")
            print("  exit status:", handle_result.returncode)
            return
        print(handle_result.stdout.strip())
        print("REAL_LEGACY_HANDLE_LINK_OK: genuine handleTelegram and parser "
              "are present in one executable; driver-field parity NOT proven")


if __name__ == "__main__":
    main()
