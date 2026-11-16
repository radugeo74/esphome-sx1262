#include "xmq_calendar_runtime.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>

static void require(bool yes,const char *message) {
  if (!yes) throw std::runtime_error(message);
}
static std::string upstream(const std::string &name) {
  std::ifstream f("experimental/xmq-runtime-stage-a/upstream/src/"+name+".xmq");
  if (!f) throw std::runtime_error("missing upstream XMQ driver");
  return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};
}
static void fixture(const xmq_runtime::Driver &driver,
                    const std::string &payload,
                    const std::string &expected_current,
                    const std::string &expected_previous) {
  using namespace xmq_runtime;
  auto records=decode_ixml_numeric(driver,parse_hex(payload));
  auto values=evaluate_numeric_fields(driver,records);
  const auto dates=evaluate_calendar_fields(driver,values);
  require(dates.at("current_date")==expected_current.c_str(),"incorrect current date");
  require(dates.at("previous_date")==expected_previous.c_str(),"incorrect previous date");
}
static bool rejects(const std::map<std::string,xmq_runtime::Number> &vars,
                    const std::string &expression) {
  try { (void)xmq_runtime::CalendarExpression(vars).evaluate(expression); }
  catch (const xmq_runtime::CalendarError &) { return true; }
  return false;
}
int main() {
  using namespace xmq_runtime;
  try {
    const auto driver=load_driver(upstream("fhkvdataiii"));
    // Original upstream fhkvdataiii fixture with a 0F proprietary payload:
    fixture(driver,
      "0F9F292D005024040011BD08380904000000070000000000000000000000000001000000000003140E",
      "2021-02-05","2020-12-31");
    // Older 01 marker from upstream fixture; no C++ branch per meter version.
    fixture(driver,
      "019F334805400CFE029808B1080000000000060F4F3467AEA9A88D7B8D503E000000000000000000",
      "2026-06-04","2025-12-31");
    // Standard 11 marker from the upstream 2020-02-08 fixture.
    fixture(driver,
      "119F27020480048300C408F709143C003D341A2B0B2A0707000000000000062D114457563D71A1850000",
      "2020-02-08","2019-12-31");

    const std::map<std::string,Number> vars{
      {"months_counter",{241,Dimension::Counter}},
      {"days_counter",{28,Dimension::Counter}}};
    const CalendarExpression calendar(vars);
    require(calendar.evaluate(
      "'2000-01-01' + (months_counter * 1month) + (days_counter * 24h)")
      =="2020-02-29","leap-year February");
    require(calendar.evaluate(
      "'2000-01-01' + (12counter * 1month) + (0counter * 24h)")
      =="2001-01-01","year boundary");
    require(rejects(vars,"'2000-02-30' + (1counter * 1month)"),
            "invalid ISO origin accepted");
    require(rejects(vars,"'2000-01-01' + (1counter * 1week)"),
            "unsupported unit accepted");
    require(rejects(vars,"'2000-01-01' + (unknown_counter * 24h)"),
            "unknown variable accepted");
    std::cout<<"CALENDAR_SUBSET_OK source-defined dates for 01,11,0F HCA variants\n";
    std::cout<<"No AES, hardware receive or full wmbusmeters equivalence claimed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr<<"FAIL: "<<e.what()<<"\n";
    return 1;
  }
}
