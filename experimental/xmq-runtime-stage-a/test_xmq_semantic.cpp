#include "xmq_semantic.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

static void check(bool ok, const char *why) {
  if (!ok) throw std::runtime_error(why);
}
static std::string source(const std::string &name) {
  std::ifstream f("experimental/xmq-runtime-stage-a/upstream/src/" + name + ".xmq");
  if (!f) throw std::runtime_error("missing upstream file: " + name);
  return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}
static const xmq_runtime::FieldRule &field(const xmq_runtime::Driver &d,
                                           const char *name) {
  for (const auto &f : d.fields) if (f.name == name && f.has_match) return f;
  throw std::runtime_error(std::string("missing matched field ") + name);
}
int main() {
  using namespace xmq_runtime;
  try {
    const Driver iperl = load_driver(source("iperl"));
    check(iperl.name == "iperl" && iperl.meter_type == "WaterMeter", "iperl metadata");
    check(detects(iperl, {"SEN", 0x68, 0x06}), "iperl SEN detect");
    check(detects(iperl, {"SEN", 0x0b, 0x07}), "iperl second model");
    check(!detects(iperl, {"TCH", 0x68, 0x06}), "manufacturer false positive");
    check(!detects(iperl, {"SEN", 0x68, 0x08}), "version false positive");
    const auto &total = field(iperl,"total");
    check(matches(total, {"0413","Instantaneous","Volume",0,0,0}),
          "iperl volume match");
    check(!matches(total, {"0413","Instantaneous","Volume",1,0,0}),
          "default storage zero");
    check(!matches(total, {"0413","Instantaneous","VolumeFlow",0,0,0}),
          "volume vs flow");

    const Driver mk4a = load_driver(source("mkradio4a"));
    check(detects(mk4a, {"TCH",0x95,0x37}), "mkradio4a Techem");
    check(detects(mk4a, {"HYD",0xfe,0x06}), "mkradio4a HYD");
    const auto &target = field(mk4a,"target");
    check(matches(target, {"4315","Instantaneous","Volume",1,0,0}),
          "mkradio4a storage 1");
    check(!matches(target, {"4315","Instantaneous","Volume",0,0,0}),
          "mkradio4a rejects storage 0");

    const Driver mk4 = load_driver(source("mkradio4"));
    check(matches(field(mk4,"prev_raw"), {"4215","Instantaneous","Volume",1,0,0}),
          "exact DIF/VIF bypasses implicit storage zero");
    check(detects(mk4, {"TCH",0x95,0x62}), "mkradio4 detect");
    check(!mk4.fields.empty(), "mkradio4 fields");

    const Driver hca = load_driver(source("fhkvdataiii"));
    check(detects(hca, {"TCH",0x69,0x80}), "hca detect");
    check(detects(hca, {"TCH",0x94,0x80}), "hca version 94");

    FieldRule unsupported;
    unsupported.has_match = true;
    unsupported.supported_match = false;
    check(!matches(unsupported, {"0413","Instantaneous","Volume",0,0,0}),
          "unknown rule must fail closed");
    const auto bad = load_driver(
        "driver { name = unsafe\n fields { field { name = foo\n"
        " match { unknown_selector = any } } } }");
    check(!bad.fields.at(0).supported_match, "unknown clause tracked");
    check(!matches(bad.fields.at(0), {"0413","Instantaneous","Volume",0,0,0}),
          "unsupported selector rejected");

    std::cout << "SEMANTIC_SUBSET_OK iperl,mkradio4a,mkradio4,fhkvdataiii\n";
    std::cout << "No telegram decoding or upstream parity claimed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL " << e.what() << "\n";
    return 1;
  }
}
