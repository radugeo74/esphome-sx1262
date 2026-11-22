#include "xmq_primary_readiness.h"
#include "xmq_ci_route_contract.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

using namespace xmq_runtime;
static void require(bool ok, const char *message) {
  if (!ok) throw std::runtime_error(message);
}
static std::string upstream() {
  std::ifstream f("experimental/xmq-runtime-stage-a/upstream/src/mkradio4a.xmq");
  if (!f) throw std::runtime_error("missing upstream mkradio4a source");
  return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}
static void run_fixture(const DriverRegistry &registry,
                        const Mvt &mvt, const char *app,
                        double expected_target) {
  // Public upstream clear DV records (NOT encrypted user telegrams).
  // Volume is Storage=1; date is OMS PointInTime Storage=1.
  const auto data = parse_hex(app);
  XmqFrameResult result;
  require(classify_xmq_telegram_route(
              0x72, ApplicationEncoding::StandardDv, true, true, false) ==
              XmqTelegramRoute::LegacyVerifiedStandard,
          "mkradio4a standard CI routing rejected");
  require(decode_primary_candidate(
              registry, mvt, ApplicationEncoding::StandardDv,
              data, "mkradio4a", {"target_m3"}, {"target_date"}, result),
          "mkradio4a XMQ primary candidate missing required date or volume");
  require(result.driver == "mkradio4a", "unexpected selected driver");
  require(std::fabs(result.numeric.at("target_m3").value-expected_target)<1e-8,
          "incorrect independently decoded mkradio4a target_m3");
  require(result.dates.at("target_date") == "2021-12-31",
          "incorrect independently decoded mkradio4a target_date");
  // Successful decoder output may not cover only volume or only date.
  require(inspect_primary_readiness(result, "mkradio4a",
                                    {"target_m3"}, {"target_date"}).ready,
          "complete source-only readiness unexpectedly rejected");
  require(!inspect_primary_readiness(result, "mkradio4",
                                     {"target_m3"}, {"target_date"}).ready,
          "a different driver could claim source output");

  XmqFrameResult denied;
  require(!decode_primary_candidate(
              registry, mvt, ApplicationEncoding::StandardDv,
              parse_hex("4315A10000"), "mkradio4a",
              {"target_m3"}, {"target_date"}, denied) &&
              denied.driver.empty() && denied.numeric.empty() &&
              denied.dates.empty(),
          "missing date record did not clear candidate");
  require(!decode_primary_candidate(
              registry, mvt, ApplicationEncoding::StandardDv,
              parse_hex("426CBF2C"), "mkradio4a",
              {"target_m3"}, {"target_date"}, denied) &&
              denied.driver.empty() && denied.numeric.empty() &&
              denied.dates.empty(),
          "missing volume record did not clear candidate");
  require(!decode_primary_candidate(
              registry, mvt, ApplicationEncoding::OriginalIxml,
              data, "mkradio4a", {"target_m3"}, {"target_date"}, denied) &&
              denied.driver.empty() && denied.numeric.empty() &&
              denied.dates.empty(),
          "standard DV was improperly interpreted as proprietary IXML");
  require(classify_xmq_telegram_route(
              0xA2, ApplicationEncoding::StandardDv, true, true, false) ==
              XmqTelegramRoute::Reject,
          "standard mkradio4a payload falsely permitted under proprietary CI");
}
int main() {
  try {
    DriverRegistry registry({load_driver(upstream())});
    // Exact public MVTs from original mkradio4a.xmq and public driver tests.
    run_fixture(registry, {"TCH",0x95,0x37}, "4315A10000426CBF2C", 16.1);
    run_fixture(registry, {"HYD",0xFE,0x06}, "4315F00200426CBF2C", 75.2);
    XmqFrameResult denied;
    require(!decode_primary_candidate(
                registry, {"TCH",0x95,0x62}, ApplicationEncoding::StandardDv,
                parse_hex("4315A10000426CBF2C"), "mkradio4a",
                {"target_m3"}, {"target_date"}, denied) &&
                denied.driver.empty() && denied.numeric.empty() &&
                denied.dates.empty(),
            "mkradio4 MVT falsely routed to mkradio4a");
    std::cout << "MKRADIO4A_PRIMARY_OK two public sources, complete date/volume,"
                 " fail-closed driver MVT and truncated data; HOST ONLY\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL " << e.what() << '\n';
    return 1;
  }
}
