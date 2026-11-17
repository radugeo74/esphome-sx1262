#include "xmq_calendar_runtime.h"
#include "xmq_oms_date.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>

static void check(bool ok, const char *why) {
  if (!ok) throw std::runtime_error(why);
}
static std::string load_source() {
  std::ifstream f("experimental/xmq-runtime-stage-a/upstream/src/fhkvdataiii.xmq");
  if (!f) throw std::runtime_error("missing original driver");
  return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}
static double get(const std::map<std::string, xmq_runtime::Number> &v,
                  const std::string &key) {
  auto it=v.find(key);
  if (it==v.end()) throw std::runtime_error("missing numeric output "+key);
  return it->second.value;
}

int main() {
  using namespace xmq_runtime;
  try {
    const auto driver=load_driver(load_source());
    // Upstream original fhkvdataiii 0x08 OMS application data after '_'.
    // The trailing 0F is a manufacturer-specific area; no proprietary
    // '01', '11', or '0F' IXML payload may be inferred from this telegram.
    const auto records=decode_dv_subset(parse_hex(
        "036E980000436E440400426C3F3C0F0F0AE388E05EADCD9772"));
    check(records.size()==3, "expected two HCA values and one standard OMS date");
    const auto oms_dates=project_oms_dates(driver,records);
    check(oms_dates.at("set_date")=="2025-12-31", "OMS date mismatch");
    const auto numbers=evaluate_numeric_fields(driver,records);
    check(std::abs(get(numbers,"current_consumption_hca")-152)<1e-9,
          "OMS current consumption should be 152 HCA");
    check(std::abs(get(numbers,"consumption_at_set_date_hca")-1092)<1e-9,
          "OMS historical consumption should be 1092 HCA");

    check(numbers.count("current_hca")==0, "proprietary HCA fabricated");
    check(numbers.count("previous_hca")==0, "proprietary previous HCA fabricated");
    check(numbers.count("previous_date_raw_counter")==0,
          "raw proprietary billing date fabricated");
    check(numbers.count("current_date_raw_counter")==0,
          "raw current date fabricated");
    check(numbers.count("previous_day_counter")==0,
          "previous day computed without source");
    check(numbers.count("current_year_counter")==0,
          "current year computed without source");

    const auto dates=evaluate_calendar_fields(driver,numbers);
    check(dates.empty(), "proprietary calendar output fabricated from missing inputs");

    // A separate full IXML frame and its decoded fields are not shared:
    // no global or static state must leak into this following OMS frame.
    const auto complete=decode_ixml_numeric(driver,parse_hex(
        "0F9F292D005024040011BD08380904000000070000000000000000000000000001000000000003140E"));
    const auto complete_nums=evaluate_numeric_fields(driver,complete);
    const auto complete_dates=evaluate_calendar_fields(driver,complete_nums);
    check(complete_dates.at("current_date")=="2021-02-05","complete date wrong");
    const auto second=evaluate_calendar_fields(driver,numbers);
    check(second.empty(),"stale date leaked between telegrams");
    check(project_oms_dates(driver,records).at("set_date")=="2025-12-31",
          "OMS date changed across frame evaluation");

    std::cout<<"OMS_PARTIAL_OK HCA consumption preserved; missing proprietary values absent\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr<<"FAIL "<<e.what()<<"\n";
    return 1;
  }
}
