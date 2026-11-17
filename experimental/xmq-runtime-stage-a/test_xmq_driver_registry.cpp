#include "xmq_driver_registry.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

static void check(bool yes,const char *why) {
  if (!yes) throw std::runtime_error(why);
}
static std::string upstream(const char *name) {
  std::ifstream f(std::string("experimental/xmq-runtime-stage-a/upstream/src/")+name+".xmq");
  if (!f) throw std::runtime_error("missing XMQ source");
  return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};
}
template<typename Func>
static bool fails(Func f) {
  try { f(); } catch (const xmq_runtime::DriverSelectionError &) { return true; }
  return false;
}
int main() {
  using namespace xmq_runtime;
  try {
    const Driver iperl=load_driver(upstream("iperl"));
    const Driver mk4a=load_driver(upstream("mkradio4a"));
    const Driver mk4=load_driver(upstream("mkradio4"));
    const Driver hca=load_driver(upstream("fhkvdataiii"));
    const DriverRegistry registry({iperl,mk4a,mk4,hca});
    check(registry.select({"SEN",0x68,0x06}).name=="iperl","iperl autodetect");
    check(registry.select({"HYD",0xFE,0x06}).name=="mkradio4a","HYD autodetect");
    check(registry.select({"TCH",0x95,0x37}).name=="mkradio4a","TCH autodetect");
    check(registry.select({"TCH",0x95,0x62}).name=="mkradio4","mkradio4 autodetect");
    check(registry.select({"TCH",0x94,0x80}).name=="fhkvdataiii","HCA autodetect");
    check(registry.select({"TCH",0x95,0x37},"mkradio4a").name=="mkradio4a",
          "explicit matching driver");
    check(fails([&] { (void)registry.select({"TCH",0x95,0x37},"iperl"); }),
          "explicit driver with wrong MVT must be rejected");
    check(fails([&] { (void)registry.select({"TCH",0x95,0x37},"unloaded"); }),
          "unloaded explicit driver must be rejected");
    check(fails([&] { (void)registry.select({"ABC",0,0}); }),
          "unknown MVT must not be guessed");
    check(fails([&] {
      const DriverRegistry ambiguous({mk4a,mk4a});
      (void)ambiguous;
    }),"duplicate driver name must be rejected");

    // Two distinct XMQ descriptions with the same detect triplet must
    // produce an ambiguity rather than choosing by file order.
    Driver overlap=mk4a;
    overlap.name="another_driver";
    const DriverRegistry ambiguous({mk4a,overlap,iperl});
    check(fails([&] { (void)ambiguous.select({"HYD",0xFE,0x06}); }),
          "overlapping MVT auto selection must be rejected");
    check(ambiguous.select({"HYD",0xFE,0x06},"mkradio4a").name=="mkradio4a",
          "explicit name resolves a real collision");

    auto water=registry.decode_standard({"SEN",0x68,0x06},
                                        parse_hex("041389E20100023B0000"));
    check(water.driver=="iperl","wrong selected driver for standard decode");
    const auto *total=find_projected(water.numeric,"total_m3");
    check(total && std::fabs(total->number-123.529)<0.0000001,
          "wrong value after driver selection");
    auto hot=registry.decode_standard({"HYD",0xFE,0x06},
                                      parse_hex("4315A10000426CBF2C"),
                                      "mkradio4a");
    check(hot.driver=="mkradio4a" && hot.dates.at("target")=="2021-12-31",
          "explicit driver clear data decode");

    std::cout<<"DRIVER_REGISTRY_OK safe MVT auto/explicit/ambiguous selection\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr<<"FAIL "<<e.what()<<"\n";
    return 1;
  }
}
