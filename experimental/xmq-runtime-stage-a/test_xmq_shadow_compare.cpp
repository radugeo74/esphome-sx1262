#include "xmq_shadow_compare.h"
#include "xmq_primary_readiness.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>

static std::string upstream(const char *name) {
  std::ifstream file(std::string("experimental/xmq-runtime-stage-a/upstream/src/")+name+".xmq");
  if (!file) throw std::runtime_error("missing original XMQ fixture");
  return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
}
static void require(bool yes,const char *why) {
  if (!yes) throw std::runtime_error(why);
}
int main() {
  using namespace xmq_runtime;
  try {
    DriverRegistry registry({
      load_driver(upstream("fhkvdataiii")),
      load_driver(upstream("mkradio4")),
      load_driver(upstream("mkradio4a")),
      load_driver(upstream("iperl"))
    });
    const auto hca=project_verified_application(
      registry,{"TCH",0x94,0x80},ApplicationEncoding::OriginalIxml,
      parse_hex("0F9F292D005024040011BD08380904000000070000000000000000000000000001000000000003140E"));
    ShadowReference hca_expected;
    hca_expected.numeric={{"current_hca",4},{"previous_hca",45},
                          {"temp_room_c",22.37},{"temp_radiator_c",23.60}};
    hca_expected.dates={{"current_date","2021-02-05"},
                        {"previous_date","2020-12-31"}};
    auto report=compare_shadow_reference(hca,hca_expected);
    require(report.equal() && report.compared==6,
            "upstream proprietary HCA reference does not match");

    // Independent XMQ-produced fields: NOT read from an old sensor snapshot.
    const auto hca_primary = inspect_primary_readiness(
        hca, "fhkvdataiii",
        {"current_hca", "previous_hca", "temp_room_c", "temp_radiator_c"},
        {"current_date", "previous_date"});
    require(hca_primary.ready && hca_primary.available == 6,
            "independent HCA primary candidate lacks required fields");
    require(!inspect_primary_readiness(
                hca, "mkradio4", {"current_hca"}).ready,
            "wrong XMQ driver accepted as primary");
    require(!inspect_primary_readiness(
                hca, "fhkvdataiii", {"current_hca", "current_hca"}).ready,
            "duplicate primary field accepted");
    require(!inspect_primary_readiness(
                hca, "fhkvdataiii", {"field_not_in_frame"}).ready,
            "missing primary output accepted");

    require(valid_primary_iso_date("2024-02-29") &&
            !valid_primary_iso_date("2025-02-29") &&
            valid_primary_iso_date("2000-02-29") &&
            !valid_primary_iso_date("1900-02-29") &&
            !valid_primary_iso_date("2025-99-99") &&
            !valid_primary_iso_date("yyyy-12-01") &&
            !valid_primary_iso_date("0000-01-01"),
            "invalid calendar date accepted as XMQ primary ready");
    auto corrupted_date = hca;
    corrupted_date.dates["current_date"] = "2025-99-99";
    require(!inspect_primary_readiness(
                corrupted_date, "fhkvdataiii",
                {"current_hca", "previous_hca", "temp_room_c", "temp_radiator_c"},
                {"current_date", "previous_date"}).ready,
            "invalid XMQ calendar output accepted for sensor publishing");
    auto corrupt_numeric = hca;
    corrupt_numeric.numeric["current_hca"].value =
        std::numeric_limits<double>::quiet_NaN();
    require(!inspect_primary_readiness(
                corrupt_numeric, "fhkvdataiii",
                {"current_hca", "previous_hca", "temp_room_c", "temp_radiator_c"},
                {"current_date", "previous_date"}).ready,
            "nonfinite XMQ numeric output accepted for sensor publishing");

    const auto oms=project_verified_application(
      registry,{"TCH",0x94,0x80},ApplicationEncoding::StandardDv,
      parse_hex("036E980000436E440400426C3F3C0F0F0AE388E05EADCD9772"));
    ShadowReference oms_expected;
    oms_expected.numeric={{"current_consumption_hca",152},
                          {"consumption_at_set_date_hca",1092}};
    oms_expected.dates={{"set_date","2025-12-31"}};
    oms_expected.absent_numeric={"current_hca","previous_hca"};
    oms_expected.absent_dates={"current_date","previous_date"};
    report=compare_shadow_reference(oms,oms_expected);
    require(report.equal() && report.compared==7,
            "OMS HCA frame leaked proprietary values or dates");

    const auto water=project_verified_application(
      registry,{"TCH",0x95,0x62},ApplicationEncoding::OriginalIxml,
      parse_hex("06702901006017030004000300000000000000000000000000000000000000000000"));
    const auto water_primary = inspect_primary_readiness(
        water, "mkradio4", {"target_m3", "total_m3"});
    require(water_primary.ready && water_primary.available == 2,
            "independent water primary candidate lacks required fields");
    require(!inspect_primary_readiness(
                water, "mkradio4", {"target_m3", "total_m3", "other"}).ready,
            "incomplete water frame accepted");
    ShadowReference mk4_expected;
    mk4_expected.numeric={{"total_m3",0.4},{"target_m3",0.1}};
    report=compare_shadow_reference(water,mk4_expected);
    require(report.equal() && report.compared==2,
            "original MK4 fixture total/target diverges");

    const auto mk4a=project_verified_application(
      registry,{"HYD",0xFE,0x06},ApplicationEncoding::StandardDv,
      parse_hex("4315A10000426CBF2C"));
    ShadowReference mk4a_expected;
    mk4a_expected.numeric={{"target_m3",16.1}};
    mk4a_expected.dates={{"target_date","2021-12-31"}};
    report=compare_shadow_reference(mk4a,mk4a_expected);
    require(report.equal() && report.compared==2,
            "original MK4A fixture target diverges");

    const auto iperl=project_verified_application(
      registry,{"SEN",0x68,0x06},ApplicationEncoding::StandardDv,
      parse_hex("2F2F0413181E0000023B00002F2F2F2F"));
    ShadowReference iperl_expected;
    iperl_expected.numeric={{"total_m3",7.704}};
    iperl_expected.absent_dates={"target_date","current_date"};
    report=compare_shadow_reference(iperl,iperl_expected);
    require(report.equal() && report.compared==3,
            "original iPERL fixture diverges");

    ShadowReference altered=hca_expected;
    altered.numeric["current_hca"]=5;
    report=compare_shadow_reference(hca,altered);
    require(!report.equal() && report.mismatches.size()==1 &&
            report.mismatches.front().field=="current_hca" &&
            report.mismatches.front().reason=="numeric differs",
            "numerical regression not detected");
    altered=hca_expected;
    altered.dates["current_date"]="2021-02-06";
    report=compare_shadow_reference(hca,altered);
    require(!report.equal() && report.mismatches.size()==1 &&
            report.mismatches.front().reason=="date differs",
            "date regression not detected");
    altered=hca_expected;
    altered.numeric["not_transmitted_hca"]=100;
    report=compare_shadow_reference(hca,altered);
    require(!report.equal() && report.mismatches.size()==1 &&
            report.mismatches.front().reason=="missing numeric",
            "missing numeric not detected");

    bool rejected=false;
    try { (void)compare_shadow_reference(hca,hca_expected,-1); }
    catch (const std::invalid_argument &) { rejected=true; }
    require(rejected,"invalid comparator tolerance accepted");

    std::cout<<"SHADOW_COMPARE_OK: 4 original XMQ drivers, proprietary and OMS parity; "
                "diff detection; absent-field checks\n";
    std::cout<<"Offline fixture parity only; no live C++ driver side-by-side execution\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr<<"FAIL: "<<e.what()<<"\n";
    return 1;
  }
}
