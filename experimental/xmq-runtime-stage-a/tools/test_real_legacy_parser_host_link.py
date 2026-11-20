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
#include <cmath>
#include <iostream>
#include <vector>

void verifyDriverLookupCreated();
extern "C" void xmq_stage_a_register_legacy_write_observer(
    void (*)(xmq_runtime::LegacyWriteReport) noexcept) noexcept;

class SyntheticMeter : public MeterCommonImplementation {
 public:
  SyntheticMeter(MeterInfo &mi, DriverInfo &di)
      : MeterCommonImplementation(mi, di) {
    // Two independent one-field host fixtures through upstream extractors.
    // These are NOT the original mkradio4 / fhkvdataiii driver definitions.
    if (di.name().str() == "fhkvdataiii") {
      addNumericFieldWithExtractor(
          "current_hca", "synthetic cleartext allocation",
          PrintProperties(DEFAULT_PRINT_PROPERTIES),
          Quantity::HCA, VifScaling::Auto, DifSignedness::Unsigned,
          FieldMatcher::build().set(VIFRange::HeatCostAllocation), Unit::HCA);
    } else {
      addNumericFieldWithExtractor(
          "total_m3", "synthetic cleartext volume",
          PrintProperties(DEFAULT_PRINT_PROPERTIES),
          Quantity::Volume, VifScaling::Auto, DifSignedness::Unsigned,
          FieldMatcher::build().set(VIFRange::Volume), Unit::M3);
    }
  }
  using MeterCommonImplementation::handleTelegram;
  using MeterCommonImplementation::getNumericValue;
};

static unsigned reports = 0;
static xmq_runtime::LegacyWriteReport last_report{};
static void record_report(xmq_runtime::LegacyWriteReport r) noexcept {
  ++reports;
  last_report = r;
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
  const double volume_m3 = meter.getNumericValue("total_m3", Unit::M3);
  const bool volume_ok = std::isfinite(volume_m3) &&
      std::fabs(volume_m3 - 4.660) < 0.000001;
  const bool observer_field_ok = reports == 1 &&
      last_report.numeric_field_mask == 2 &&
      last_report.numeric_writes == 1 &&
      last_report.dropped_writes == 0 &&
      last_report.string_writes == 0;
  std::cout << "REAL_LEGACY_HANDLE_EXECUTED: handled=" << handled
            << " matched=" << matched
            << " reports=" << reports
            << " updates=" << meter.numUpdates()
            << " numeric_field_mask=" << last_report.numeric_field_mask
            << " numeric_writes=" << last_report.numeric_writes
            << " volume_ok=" << volume_ok << "\n";
  // No real driver-specific extractor is configured, so no field-value parity
  // is implied. Still require one actual observer dispatch for the accepted
  // synthetic telegram, one legacy update and a positive meter ID match.
  if (!handled || !matched || !observer_field_ok || !volume_ok ||
      meter.numUpdates() != 1) {
    std::cerr << "REAL_LEGACY_HANDLE_FLOW_FAILED: parser/update/observer contract\n";
    return 11;
  }
  std::cout << "REAL_LEGACY_HANDLE_FLOW_OK: original parser, real DV"
               " extractor, numeric setter, observer and update executed\n";

  // A second fixture exercises real upstream HCA VIF (0x6E) extraction.
  // No measurement is copied from a real heat-cost allocator.
  DriverInfo heat_driver;
  heat_driver.setName("fhkvdataiii");
  heat_driver.setMeterType(MeterType::HeatCostAllocationMeter);
  SyntheticMeter heat_meter(mi, heat_driver);
  std::vector<uchar> heat_frame = {
    0x0E, 0x44, 0x33, 0x30, 0x01, 0x02, 0x03, 0x04,
    0x01, 0x07, 0x78, 0x02, 0x6E, 0x34, 0x12
  };
  reports = 0;
  last_report = {};
  addresses.clear();
  matched = false;
  xmq_stage_a_register_legacy_write_observer(record_report);
  const bool heat_handled = heat_meter.handleTelegram(
      about, heat_frame, false, &addresses, &matched, nullptr);
  xmq_stage_a_register_legacy_write_observer(nullptr);
  const double hca_value = heat_meter.getNumericValue("current_hca", Unit::HCA);
  const bool hca_ok = std::isfinite(hca_value) &&
      std::fabs(hca_value - 4660.0) < 0.000001;
  const bool heat_observer_ok = reports == 1 &&
      last_report.numeric_field_mask == 1 &&
      last_report.numeric_writes == 1 &&
      last_report.string_writes == 0 &&
      last_report.dropped_writes == 0;
  std::cout << "REAL_LEGACY_HCA_EXECUTED: handled=" << heat_handled
            << " matched=" << matched
            << " reports=" << reports
            << " updates=" << heat_meter.numUpdates()
            << " numeric_field_mask=" << last_report.numeric_field_mask
            << " hca_ok=" << hca_ok << "\n";
  if (!heat_handled || !matched || !heat_observer_ok || !hca_ok ||
      heat_meter.numUpdates() != 1) {
    std::cerr << "REAL_LEGACY_HCA_FLOW_FAILED: original HCA DV extraction or"
                 " numeric setter attribution differs\n";
    return 12;
  }
  std::cout << "REAL_LEGACY_HCA_FLOW_OK: 0x6E HCA VIF decoded with original"
               " field extractor and attributed to current_hca\n";

  // Load and instantiate the ORIGINAL upstream driver source translation units,
  // not the hand-authored single-field SyntheticMeter used above.
  // This is a registry/constructor/field-schema check, NOT driver payload
  // parity: CI=0xA0 / CI=0xA2 encrypted or vendor payloads come later.
  auto has_field = [](Meter &m, const char *field) {
    for (auto &fi : m.fieldInfos())
      if (fi.vname() == field) return true;
    return false;
  };
  for (const char *name : {"mkradio4", "fhkvdataiii"}) {
    DriverInfo *di = lookupDriver(name);
    if (di == nullptr || !di->hasProcessContent()) {
      std::cerr << "REAL_LEGACY_DRIVER_REGISTRY_FAILED: upstream definition missing\n";
      return 13;
    }
    MeterInfo driver_mi;
    driver_mi.name = "synthetic-upstream-driver";
    driver_mi.driver_name = DriverName(name);
    driver_mi.poll_interval = 0;
    driver_mi.address_expressions.push_back(matcher);
    auto original_driver = createMeter(&driver_mi);
    if (!original_driver ||
        (std::string(name) == "mkradio4" &&
         (!has_field(*original_driver, "total") ||
          !has_field(*original_driver, "target"))) ||
        (std::string(name) == "fhkvdataiii" &&
         (!has_field(*original_driver, "current") ||
          !has_field(*original_driver, "previous") ||
          !has_field(*original_driver, "temp_room") ||
          !has_field(*original_driver, "temp_radiator")))) {
      std::cerr << "REAL_LEGACY_DRIVER_CONSTRUCTOR_FAILED: expected original"
                   " fields absent\n";
      return 14;
    }
    std::cout << "REAL_LEGACY_DRIVER_CONSTRUCTED_OK: "
              << name << " upstream constructor and field definitions\n";
  }

  // Exercise the ORIGINAL mkradio4 proprietary processContent() in the same
  // strict executable. Assemble an entirely artificial CI=0xA2 telegram:
  // synthetic DLL identity, synthetic target=12.3 m3, delta=4.5 m3.
  // This is a host fixture, not a copied meter capture.
  MeterInfo vendor_mi;
  vendor_mi.name = "synthetic-a2-driver-fixture";
  vendor_mi.driver_name = DriverName("mkradio4");
  vendor_mi.poll_interval = 0;
  vendor_mi.address_expressions.push_back(matcher);
  auto vendor_water = createMeter(&vendor_mi);
  if (!vendor_water) return 15;
  std::vector<uchar> vendor_frame = {
    0x2F, 0x44, 0x68, 0x50, 0x01, 0x02, 0x03, 0x04,
    0x95, 0x62, 0xA2
  };
  vendor_frame.resize(48, 0);
  // Proprietary content offsets in the untouched upstream mkradio4 driver:
  // bytes 3/4 = target * 10; bytes 7/8 = additional volume * 10.
  vendor_frame[11 + 3] = 0x7B; // 123 => 12.3 m3
  vendor_frame[11 + 4] = 0x00;
  vendor_frame[11 + 7] = 0x2D; // 45 => 4.5 m3
  vendor_frame[11 + 8] = 0x00;
  reports = 0;
  last_report = {};
  addresses.clear();
  matched = false;
  xmq_stage_a_register_legacy_write_observer(record_report);
  const bool vendor_handled = vendor_water->handleTelegram(
      about, vendor_frame, false, &addresses, &matched, nullptr);
  xmq_stage_a_register_legacy_write_observer(nullptr);
  const double target = vendor_water->getNumericValue("target", Unit::M3);
  const double total = vendor_water->getNumericValue("total", Unit::M3);
  const bool vendor_values_ok = std::isfinite(target) &&
      std::isfinite(total) &&
      std::fabs(target - 12.3) < 0.000001 &&
      std::fabs(total - 16.8) < 0.000001;
  const bool vendor_observer_ok = reports == 1 &&
      last_report.numeric_field_mask == 3 &&
      last_report.numeric_writes == 2 &&
      last_report.string_writes == 0 &&
      last_report.dropped_writes == 0;
  std::cout << "REAL_MKRADIO4_PROCESS_CONTENT_EXECUTED: handled=" << vendor_handled
            << " matched=" << matched
            << " reports=" << reports
            << " updates=" << vendor_water->numUpdates()
            << " mask=" << last_report.numeric_field_mask
            << " values_ok=" << vendor_values_ok << "\n";
  if (!vendor_handled || !matched || !vendor_values_ok ||
      !vendor_observer_ok || vendor_water->numUpdates() != 1) {
    std::cerr << "REAL_MKRADIO4_PROCESS_CONTENT_FAILED:"
                 " upstream A2 numeric fields/observer mismatch\n";
    return 16;
  }
  std::cout << "REAL_MKRADIO4_PROCESS_CONTENT_OK:"
               " target_m3=12.3 total_m3=16.8 via original driver\n";

  // Drive ORIGINAL fhkvdataiii manufacturer-specific CI=A0 processing
  // through a wholly synthetic, CRC-stripped DLL telegram (v0x69).
  // Dates: previous 2024-05-15; current 2024-06-16.
  // Consumption: previous 1250 HCA, current 312 HCA.
  // Temperatures: room 22.44 C, radiator 25.51 C.
  MeterInfo proprietary_heat_mi;
  proprietary_heat_mi.name = "synthetic-a0-driver-fixture";
  proprietary_heat_mi.driver_name = DriverName("fhkvdataiii");
  proprietary_heat_mi.poll_interval = 0;
  proprietary_heat_mi.address_expressions.push_back(matcher);
  auto proprietary_heat = createMeter(&proprietary_heat_mi);
  if (!proprietary_heat) return 17;
  std::vector<uchar> proprietary_heat_frame = {
    0x18, 0x44, 0x68, 0x50, 0x01, 0x02, 0x03, 0x04,
    0x69, 0x80, 0xA0,
    0x00, 0xAF, 0x30, 0xE2, 0x04, 0x00, 0x0D, 0x38, 0x01,
    0xC4, 0x08, 0xF7, 0x09, 0x00
  };
  if (proprietary_heat_frame.size() != 25) return 18;
  reports = 0;
  last_report = {};
  addresses.clear();
  matched = false;
  xmq_stage_a_register_legacy_write_observer(record_report);
  const bool proprietary_heat_handled = proprietary_heat->handleTelegram(
      about, proprietary_heat_frame, false, &addresses, &matched, nullptr);
  xmq_stage_a_register_legacy_write_observer(nullptr);
  const double current_hca = proprietary_heat->getNumericValue("current", Unit::HCA);
  const double previous_hca = proprietary_heat->getNumericValue("previous", Unit::HCA);
  const double temp_room = proprietary_heat->getNumericValue("temp_room", Unit::C);
  const double temp_radiator = proprietary_heat->getNumericValue("temp_radiator", Unit::C);
  std::string previous_date;
  std::string current_date;
  for (auto &field : proprietary_heat->fieldInfos()) {
    if (field.vname() == "previous_date")
      previous_date = proprietary_heat->getStringValue(&field);
    if (field.vname() == "current_date")
      current_date = proprietary_heat->getStringValue(&field);
  }
  const bool heat_values_ok =
      std::isfinite(current_hca) && std::fabs(current_hca - 312.0) < 0.000001 &&
      std::isfinite(previous_hca) && std::fabs(previous_hca - 1250.0) < 0.000001 &&
      std::isfinite(temp_room) && std::fabs(temp_room - 22.44) < 0.000001 &&
      std::isfinite(temp_radiator) && std::fabs(temp_radiator - 25.51) < 0.000001 &&
      previous_date == "2024-05-15T02:00:00Z" &&
      current_date == "2024-06-16T02:00:00Z";
  const bool proprietary_heat_observer_ok = reports == 1 &&
      last_report.numeric_field_mask == 0x0F &&
      last_report.date_field_mask == 0x03 &&
      last_report.numeric_writes == 4 &&
      last_report.string_writes == 2 &&
      last_report.dropped_writes == 0;
  std::cout << "REAL_FHKVDATAIII_PROCESS_CONTENT_EXECUTED: handled="
            << proprietary_heat_handled << " matched=" << matched
            << " reports=" << reports
            << " updates=" << proprietary_heat->numUpdates()
            << " numeric_mask=" << last_report.numeric_field_mask
            << " date_mask=" << last_report.date_field_mask
            << " numeric_writes=" << last_report.numeric_writes
            << " string_writes=" << last_report.string_writes
            << " values_ok=" << heat_values_ok << "\n";
  if (!proprietary_heat_handled || !matched || !heat_values_ok ||
      !proprietary_heat_observer_ok || proprietary_heat->numUpdates() != 1) {
    std::cerr << "REAL_FHKVDATAIII_PROCESS_CONTENT_FAILED:"
                 " original A0 HCA/date/temperature or observer mismatch\n";
    return 19;
  }
  std::cout << "REAL_FHKVDATAIII_PROCESS_CONTENT_OK:"
               " original proprietary HCA/date/temperature fields and"
               " XMQ observer agree on synthetic fixture\n";

  // Version 0x94 has an extra manufacturer byte before temperature fields.
  // Preserve the independent 0x69 test above; assert 0x94 reads temperature
  // bytes from offset 10, not offset 9.
  MeterInfo version94_mi;
  version94_mi.name = "synthetic-a0-v94-driver-fixture";
  version94_mi.driver_name = DriverName("fhkvdataiii");
  version94_mi.poll_interval = 0;
  version94_mi.address_expressions.push_back(matcher);
  auto version94 = createMeter(&version94_mi);
  if (!version94) return 20;
  std::vector<uchar> version94_frame = {
    0x19, 0x44, 0x68, 0x50, 0x01, 0x02, 0x03, 0x04,
    0x94, 0x80, 0xA0,
    0x00, 0xAF, 0x30, 0xE2, 0x04, 0x00, 0x0D, 0x38, 0x01,
    0xEE, // Version 0x94-only extra byte before temperatures.
    0xC4, 0x08, 0xF7, 0x09, 0x00
  };
  if (version94_frame.size() != 26) return 21;
  reports = 0;
  last_report = {};
  addresses.clear();
  matched = false;
  xmq_stage_a_register_legacy_write_observer(record_report);
  const bool handled94 = version94->handleTelegram(
      about, version94_frame, false, &addresses, &matched, nullptr);
  xmq_stage_a_register_legacy_write_observer(nullptr);
  const double hca94 = version94->getNumericValue("current", Unit::HCA);
  const double prev94 = version94->getNumericValue("previous", Unit::HCA);
  const double room94 = version94->getNumericValue("temp_room", Unit::C);
  const double radiator94 = version94->getNumericValue("temp_radiator", Unit::C);
  std::string previous_date94;
  std::string current_date94;
  for (auto &field : version94->fieldInfos()) {
    if (field.vname() == "previous_date")
      previous_date94 = version94->getStringValue(&field);
    if (field.vname() == "current_date")
      current_date94 = version94->getStringValue(&field);
  }
  const bool values94_ok =
      std::isfinite(hca94) && std::fabs(hca94 - 312.0) < 0.000001 &&
      std::isfinite(prev94) && std::fabs(prev94 - 1250.0) < 0.000001 &&
      std::isfinite(room94) && std::fabs(room94 - 22.44) < 0.000001 &&
      std::isfinite(radiator94) && std::fabs(radiator94 - 25.51) < 0.000001 &&
      previous_date94 == "2024-05-15T02:00:00Z" &&
      current_date94 == "2024-06-16T02:00:00Z";
  const bool observer94_ok = reports == 1 &&
      last_report.numeric_field_mask == 0x0F &&
      last_report.date_field_mask == 0x03 &&
      last_report.numeric_writes == 4 &&
      last_report.string_writes == 2 &&
      last_report.dropped_writes == 0;
  std::cout << "REAL_FHKVDATAIII_V94_EXECUTED: handled=" << handled94
            << " matched=" << matched << " reports=" << reports
            << " updates=" << version94->numUpdates()
            << " numeric_mask=" << last_report.numeric_field_mask
            << " date_mask=" << last_report.date_field_mask
            << " numeric_writes=" << last_report.numeric_writes
            << " string_writes=" << last_report.string_writes
            << " values_ok=" << values94_ok << "\n";
  if (!handled94 || !matched || !values94_ok || !observer94_ok ||
      version94->numUpdates() != 1) {
    std::cerr << "REAL_FHKVDATAIII_V94_FAILED: offset 10 proprietary"
                 " temperature, HCA, date or observer mismatch\n";
    return 22;
  }
  std::cout << "REAL_FHKVDATAIII_V94_OK: original driver v0x94"
               " extra-byte temperature offset verified\n";

  // Reject a correctly formed telegram when the original driver's configured
  // address is NOT the synthetic DLL address. No parser bypass or manual
  // mocking of isTelegramForMeter() is allowed.
  AddressExpression foreign_only;
  if (!foreign_only.parse("99999999")) return 23;
  for (const char *driver_name : {"mkradio4", "fhkvdataiii"}) {
    MeterInfo foreign_mi;
    foreign_mi.name = "synthetic-foreign-address-filter";
    foreign_mi.driver_name = DriverName(driver_name);
    foreign_mi.poll_interval = 0;
    foreign_mi.address_expressions.push_back(foreign_only);
    auto filtered_meter = createMeter(&foreign_mi);
    if (!filtered_meter) return 24;
    const auto &foreign_frame = std::string(driver_name) == "mkradio4"
        ? vendor_frame : proprietary_heat_frame;
    reports = 0;
    last_report = {};
    addresses.clear();
    matched = false;
    xmq_stage_a_register_legacy_write_observer(record_report);
    const bool accepted = filtered_meter->handleTelegram(
        about, foreign_frame, false, &addresses, &matched, nullptr);
    xmq_stage_a_register_legacy_write_observer(nullptr);
    const bool blocked = !accepted && !matched && reports == 0 &&
        filtered_meter->numUpdates() == 0 &&
        last_report.numeric_writes == 0 &&
        last_report.string_writes == 0;
    std::cout << "REAL_LEGACY_FOREIGN_METER_EXECUTED: driver="
              << driver_name << " blocked=" << blocked
              << " reports=" << reports
              << " updates=" << filtered_meter->numUpdates() << "\n";
    if (!blocked) {
      std::cerr << "REAL_LEGACY_FOREIGN_METER_FAILED: foreign DLL address"
                   " reached original decoder/observer/update\n";
      return 25;
    }
  }
  std::cout << "REAL_LEGACY_FOREIGN_METER_OK: original heat and water"
               " drivers reject foreign addresses before any writes\n";
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
        # Compile genuine upstream manufacturer drivers; their static
        # registrars and original constructors must run in the handle probe.
        driver_objects = []
        for driver_unit in ("driver_mkradio4", "driver_fhkvdataiii"):
            source = COMMON / (driver_unit + ".cc")
            target = temp / (driver_unit + ".o")
            run([
                "g++", "-std=c++17", "-O0", "-g0", "-ffunction-sections",
                "-fdata-sections", "-Wno-unused-parameter", "-c",
                "-I" + str(COMMON), "-I" + str(HERE / "host-stubs"),
                str(source), "-o", str(target),
            ], check=True)
            driver_objects.append(str(target))
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
            str(handle_obj), *objects, *driver_objects, "-o", str(handle_bin)
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
            print("REAL_LEGACY_HANDLE_LINK_FAILED: full decoder executable "
                  "has unresolved legacy dependencies:", len(missing))
            for symbol in missing[:20]:
                print("  handle-unresolved:", symbol[:220])
            print("Unlisted additional symbols:", max(0, len(missing) - 20))
            raise SystemExit("REAL_LEGACY_HANDLE_LINK_FAILED: real handleTelegram gate failed")
        handle_result = run([str(handle_bin)], capture_output=True)
        if handle_result.returncode:
            # The probe contains synthetic bytes/IDs only. Surface only its
            # structured status lines, never raw payloads or private secrets.
            for line in handle_result.stdout.splitlines():
                if line.startswith(("REAL_LEGACY_", "REAL_MKRADIO4_")):
                    print(line, flush=True)
            raise RuntimeError("REAL_LEGACY_HANDLE_FLOW_FAILED: synthetic "
                               "parser/observer/update execution regressed; "
                               "exit status=" + str(handle_result.returncode))
        print(handle_result.stdout.strip())
        required_markers = (
            "REAL_LEGACY_HANDLE_FLOW_OK:",
            "REAL_LEGACY_HCA_FLOW_OK:",
            "numeric_field_mask=2 numeric_writes=1 volume_ok=1",
            "numeric_field_mask=1 hca_ok=1",
            "REAL_LEGACY_DRIVER_CONSTRUCTED_OK: mkradio4",
            "REAL_LEGACY_DRIVER_CONSTRUCTED_OK: fhkvdataiii",
            "REAL_MKRADIO4_PROCESS_CONTENT_OK:",
            "REAL_FHKVDATAIII_PROCESS_CONTENT_OK:",
            "REAL_FHKVDATAIII_V94_OK:",
            "REAL_LEGACY_FOREIGN_METER_OK:",
        )
        for marker in required_markers:
            if marker not in handle_result.stdout:
                raise RuntimeError("REAL_LEGACY_HANDLE_FLOW_FAILED: missing "
                                   "expected volume/HCA numeric extraction "
                                   "or write-observer success marker")
        print("REAL_LEGACY_HANDLE_LINK_OK: genuine parser, handleTelegram, "
              "synthetic volume/HCA extractors and write observer executed; "
              "original driver-field parity NOT proven")


if __name__ == "__main__":
    main()
