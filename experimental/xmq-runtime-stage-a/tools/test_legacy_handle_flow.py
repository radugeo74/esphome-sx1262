#!/usr/bin/env python3
"""Execute the verbatim staged legacy handleTelegram orchestration on host.

Parser, meter, fields and RF frames are deliberately synthetic stand-ins.
This verifies legacy control flow and real staging hooks; it is NOT decoding
an actual OMS telegram nor linking the complete wmbus_common library.
"""
from pathlib import Path
import subprocess
import tempfile
from stage_legacy_setter_instrumentation import SOURCE, instrument
from test_legacy_setter_execution import extract_method

HERE = Path(__file__).resolve().parent
STAGE = HERE.parent

def main():
    staged = instrument(SOURCE.read_text())
    handle = extract_method(
        staged, "bool MeterCommonImplementation::handleTelegram(")
    numeric = extract_method(
        staged, "void MeterCommonImplementation::setNumericValue(FieldInfo *fi, DVEntry *dve,")
    string = extract_method(
        staged, "void MeterCommonImplementation::setStringValue(FieldInfo *fi, std::string v,")
    start = staged.index("namespace {\nthread_local xmq_runtime::LegacyWriteJournal *lab_journal")
    callback = staged.index("  lab_bound_observer = callback;", start)
    end = staged.index("\n}", callback)
    scope = staged[start:end + 2]
    for anchor in ("LabWriteScope lab_write_scope(lab_writes);",
                   "processFieldExtractors(&t);", "processFieldCalculators();",
                   "dispatch_legacy_write_report(", "triggerUpdate(&t);"):
        if anchor not in handle:
            raise ValueError("handleTelegram staging drift: " + anchor)
    if not (handle.index("processFieldExtractors(&t);") <
            handle.index("dispatch_legacy_write_report(") <
            handle.index("triggerUpdate(&t);")):
        raise ValueError("observer not between decode and update")

    header = r"""
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include <stdexcept>
#include <iostream>
#include "xmq_legacy_write_observer.h"
using uchar = unsigned char;
enum class Unit { Unknown, Unitless };
struct DVEntry {};
struct Address {
  std::string str() const { return "synthetic-address"; }
};
struct AboutTelegram {};
struct MeterKeys {};
struct MeterCommonImplementation;
struct FieldInfo {
  std::string field;
  std::string vname() const { return field; }
  Unit displayUnit() const { return Unit::Unitless; }
  std::string generateFieldNameNoUnit(MeterCommonImplementation *, DVEntry *) const {
    return field + "_dv";
  }
};
struct NumericField {
  Unit unit{};
  double value{};
  FieldInfo *fi{};
  NumericField() = default;
  NumericField(Unit u, double v, FieldInfo *f) : unit(u), value(v), fi(f) {}
  NumericField(Unit u, double v, FieldInfo *f, DVEntry &) : unit(u), value(v), fi(f) {}
};
struct StringField {
  std::string value;
  FieldInfo *fi{};
  StringField() = default;
  StringField(std::string s, FieldInfo *f) : value(std::move(s)), fi(f) {}
};
struct DriverName {
  std::string value;
  std::string str() const { return value; }
};
struct Telegram {
  AboutTelegram about{};
  std::vector<Address> addresses{};
  std::vector<uchar> original{};
  std::vector<uchar> frame{};
  int header_size{};
  int suffix_size{};
  int force_mfct_index{-1};
  bool simulated{};
  bool analyzed{};
  bool parseHeader(const std::vector<uchar> &data) {
    if (data.empty() || data[0] != 0xA1) return false;
    addresses.push_back(Address{});
    return true;
  }
  void markAsSimulated() { simulated = true; }
  void markAsBeingAnalyzed() { analyzed = true; }
  bool parse(const std::vector<uchar> &data, MeterKeys *, bool) {
    if (data.size() < 2 || data[1] != 0x01) return false;
    frame = data;
    original = data;
    return true;
  }
};
#define verbose(...) ((void)0)
#define debug(...) ((void)0)
static std::string bin2hex(const std::vector<uchar> &) { return "synthetic"; }
struct MeterCommonImplementation {
  std::map<std::pair<std::string, Unit>, NumericField> numeric_values_;
  std::map<std::string, StringField> string_values_;
  MeterKeys meter_keys_{};
  int force_mfct_index_{-1};
  std::string driver_{"fhkvdataiii"};
  int updates{};
  int extractor_calls{};
  int content_calls{};
  int calculator_calls{};
  bool has_content{true};
  FieldInfo heat{"current_hca"};
  FieldInfo water{"total_m3"};
  FieldInfo date{"previous_date"};
  std::string name() { return "synthetic"; }
  int index() { return 0; }
  DriverName driverName() { return DriverName{driver_}; }
  static bool isTelegramForMeter(Telegram *, MeterCommonImplementation *, void *) {
    return true;
  }
  bool hasProcessContent() { return has_content; }
  void setNumericValue(FieldInfo *, DVEntry *, Unit, double);
  void setStringValue(FieldInfo *, std::string, DVEntry *);
  void processFieldExtractors(Telegram *) {
    ++extractor_calls;
    if (driver_ == "mkradio4")
      setNumericValue(&water, nullptr, Unit::Unitless, 91.3);
    else
      setNumericValue(&heat, nullptr, Unit::Unitless, 42.0);
  }
  void processContent(Telegram *) {
    ++content_calls;
    if (driver_ == "fhkvdataiii")
      setStringValue(&date, "synthetic", nullptr);
  }
  void processFieldCalculators() { ++calculator_calls; }
  void triggerUpdate(Telegram *) { ++updates; }
  bool handleTelegram(AboutTelegram &, std::vector<uchar>, bool,
                      std::vector<Address> *, bool *, Telegram *);
};
"""
    tests = r"""
static unsigned callbacks{};
static xmq_runtime::LegacyWriteReport last{};
static void on_report(xmq_runtime::LegacyWriteReport r) noexcept {
  ++callbacks;
  last = r;
}
static void require(bool condition, const char *msg) {
  if (!condition) throw std::runtime_error(msg);
}
int main() {
  try {
    MeterCommonImplementation meter;
    AboutTelegram about;
    std::vector<Address> addresses;
    bool matched = false;
    Telegram analyzed{};
    xmq_stage_a_register_legacy_write_observer(on_report);
    // Early parseHeader failure: no callback, extractor or update.
    require(!meter.handleTelegram(about, {0x00, 0x01}, false,
                                  &addresses, &matched, &analyzed),
            "header failure accepted");
    require(callbacks == 0 && meter.updates == 0 && meter.extractor_calls == 0,
            "header failure polluted provenance");
    // Parsed header but failed payload: no callback.
    require(!meter.handleTelegram(about, {0xA1, 0x00}, true,
                                  &addresses, &matched, &analyzed),
            "payload failure accepted");
    require(callbacks == 0 && meter.updates == 0 && meter.extractor_calls == 0,
            "payload failure polluted provenance");
    require(analyzed.simulated && analyzed.analyzed,
            "simulated analyze flags were lost");
    // Heat: exact staged call chain, real setter bodies and observer.
    require(meter.handleTelegram(about, {0xA1, 0x01}, false,
                                 &addresses, &matched, &analyzed),
            "heat frame rejected");
    require(matched && callbacks == 1 && last.numeric_field_mask == 1 &&
            last.date_field_mask == 2 && last.numeric_writes == 1 &&
            last.string_writes == 1 && meter.updates == 1,
            "heat field provenance/update failed");
    // Same value in a new telegram must remain a new write.
    require(meter.handleTelegram(about, {0xA1, 0x01}, false,
                                 &addresses, &matched, &analyzed),
            "repeated heat frame rejected");
    require(callbacks == 2 && last.numeric_writes == 1 &&
            last.string_writes == 1, "repeated heat frame was not observed");
    // Water: no heat date allowed to carry over.
    meter.driver_ = "mkradio4";
    require(meter.handleTelegram(about, {0xA1, 0x01}, false,
                                 &addresses, &matched, &analyzed),
            "water frame rejected");
    require(callbacks == 3 && last.numeric_field_mask == 2 &&
            last.date_field_mask == 0 && last.numeric_writes == 1 &&
            last.string_writes == 0 && meter.updates == 3,
            "water field provenance leaked previous heat writes");
    xmq_stage_a_register_legacy_write_observer(nullptr);
    require(meter.handleTelegram(about, {0xA1, 0x01}, false,
                                 &addresses, &matched, &analyzed),
            "frame with disabled observer rejected");
    require(callbacks == 3 && meter.updates == 4,
            "disabled observer changed decoding/update flow");
    require(meter.extractor_calls == 4 && meter.content_calls == 4 &&
            meter.calculator_calls == 4, "decode call sequence incorrect");
    std::cout << "LEGACY_HANDLE_FLOW_OK: exact handleTelegram body and setter bodies, "
                 "decode failure guards, heat/water callbacks, repeated writes\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL: " << e.what() << '\n';
    return 1;
  }
}
"""
    with tempfile.TemporaryDirectory(prefix="xmq-handle-") as tmp:
        src = Path(tmp) / "legacy-handle-flow.cc"
        exe = Path(tmp) / "legacy-handle-flow"
        src.write_text(header + "\n" + scope + "\n" + numeric + "\n" +
                       string + "\n" + handle + "\n" + tests)
        subprocess.run(
            ["g++", "-std=c++17", "-O1", "-g", "-Wall", "-Wextra",
             "-Werror", "-pedantic", "-fsanitize=address,undefined",
             "-fno-omit-frame-pointer", "-I", str(STAGE),
             str(src), "-o", str(exe)],
            check=True,
        )
        subprocess.run([str(exe)], check=True)

if __name__ == "__main__":
    main()
