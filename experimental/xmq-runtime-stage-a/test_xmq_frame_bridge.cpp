#include "xmq_frame_bridge.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

static std::string original(const std::string &name) {
  std::ifstream f("experimental/xmq-runtime-stage-a/upstream/src/"+name+".xmq");
  if (!f) throw std::runtime_error("missing upstream XMQ source");
  return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};
}
static void require(bool yes, const char *why) {
  if (!yes) throw std::runtime_error(why);
}
static void number(const xmq_runtime::XmqFrameResult &r,
                   const char *name,double expected) {
  const auto it=r.numeric.find(name);
  if (it==r.numeric.end() || std::fabs(it->second.value-expected)>1e-7)
    throw std::runtime_error(std::string("wrong/missing numerical field ")+name);
}
template<typename F>
static bool rejected(F fn) {
  try { fn(); }
  catch (const std::exception &) { return true; }
  return false;
}
int main() {
  using namespace xmq_runtime;
  try {
    DriverRegistry registry({
      load_driver(original("fhkvdataiii")),
      load_driver(original("mkradio4")),
      load_driver(original("mkradio4a")),
      load_driver(original("iperl"))
    });
    // Original upstream manufacturer-specific 0F payload.
    const auto hca=project_verified_application(
      registry,{"TCH",0x94,0x80},ApplicationEncoding::OriginalIxml,
      parse_hex("0F9F292D005024040011BD08380904000000070000000000000000000000000001000000000003140E"));
    require(hca.driver=="fhkvdataiii","wrong HCA driver");
    number(hca,"current_hca",4);
    number(hca,"previous_hca",45);
    number(hca,"temp_room_c",22.37);
    number(hca,"temp_radiator_c",23.60);
    require(hca.dates.at("current_date")=="2021-02-05","wrong HCA current date");
    require(hca.dates.at("previous_date")=="2020-12-31","wrong HCA previous date");

    // Partial standard HCA telegram: values are real, proprietary dates absent.
    const auto standard_hca=project_verified_application(
      registry,{"TCH",0x94,0x80},ApplicationEncoding::StandardDv,
      parse_hex("036E980000436E440400426C3F3C0F0F0AE388E05EADCD9772"));
    number(standard_hca,"current_consumption_hca",152);
    number(standard_hca,"consumption_at_set_date_hca",1092);
    require(standard_hca.dates.at("set_date")=="2025-12-31",
            "missing standard billing date");
    require(!standard_hca.dates.count("current_date") &&
            !standard_hca.dates.count("previous_date"),
            "proprietary HCA dates invented for OMS payload");

    // Proprietary MK radio 4 uses source-defined IXML and expressions.
    const auto mk4=project_verified_application(
      registry,{"TCH",0x95,0x62},ApplicationEncoding::OriginalIxml,
      parse_hex("06702901006017030004000300000000000000000000000000000000000000000000"));
    number(mk4,"target_m3",0.1);
    number(mk4,"total_m3",0.4);

    // Standard MK radio 4a matched fields and date.
    const auto mk4a=project_verified_application(
      registry,{"HYD",0xFE,0x06},ApplicationEncoding::StandardDv,
      parse_hex("4315A10000426CBF2C"));
    number(mk4a,"target_m3",16.1);
    require(mk4a.dates.at("target_date")=="2021-12-31",
            "standard target_date missing or renamed");

    // Plain standard iPERL.
    const auto iperl=project_verified_application(
      registry,{"SEN",0x68,0x06},ApplicationEncoding::StandardDv,
      parse_hex("041389E20100023B0000"));
    number(iperl,"total_m3",123.529);
    require(iperl.dates.empty(),"fabricated iPERL date");

    require(rejected([&] {
      (void)project_verified_application(registry,{"SEN",0x68,0x06},
           ApplicationEncoding::OriginalIxml,parse_hex("041389E20100023B0000"));
    }),"wrong IXML encoding accepted");
    require(rejected([&] {
      (void)project_verified_application(registry,{"HYD",0xFE,0x06},
           ApplicationEncoding::StandardDv,parse_hex("4315A10000426CBF2C"),"iperl");
    }),"mismatched driver accepted");
    require(rejected([&] {
      (void)project_verified_application(registry,{"ABC",0,0},
           ApplicationEncoding::StandardDv,parse_hex("4315A10000426CBF2C"));
    }),"unknown MVT accepted");
    require(rejected([&] {
      (void)project_verified_application(registry,{"HYD",0xFE,0x06},
           ApplicationEncoding::StandardDv,{});
    }),"empty application payload accepted");
    require(rejected([&] {
      (void)project_verified_application(registry,{"HYD",0xFE,0x06},
           static_cast<ApplicationEncoding>(99),parse_hex("4315A10000426CBF2C"));
    }),"unknown application encoding accepted");

    std::cout<<"FRAME_BRIDGE_OK original HCA, MK4, MK4A, iPERL fixtures; "
                "strict clear-application boundaries\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr<<"FAIL: "<<e.what()<<"\n";
    return 1;
  }
}
