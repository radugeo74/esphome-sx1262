#include "xmq_clear_runtime.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

static void verify(bool ok,const char *message) {
  if (!ok) throw std::runtime_error(message);
}
static std::string upstream(const char *name) {
  const std::string path=std::string("experimental/xmq-runtime-stage-a/upstream/src/")+name+".xmq";
  std::ifstream file(path);
  if (!file) throw std::runtime_error("missing original XMQ file");
  return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
}
static void numeric(const xmq_runtime::ClearResult &result,
                    const char *name,double wanted) {
  const auto *record=xmq_runtime::find_projected(result.numeric,name);
  verify(record!=nullptr,"missing numerical XMQ field");
  verify(std::fabs(record->number-wanted)<0.0000001,"incorrect numerical XMQ value");
}
int main() {
  using namespace xmq_runtime;
  try {
    // Original iperl.xmq application fixture: same XMQ file, two payloads.
    const auto iperl=load_driver(upstream("iperl"));
    auto water=decode_standard_clear(iperl,{"SEN",0x68,0x06},
                                    parse_hex("2F2F0413181E0000023B00002F2F2F2F"));
    verify(water.driver=="iperl","wrong driver returned");
    numeric(water,"total_m3",7.704);
    numeric(water,"max_flow_m3h",0.0);
    verify(water.dates.empty(),"invented date on iperl frame");
    water=decode_standard_clear(iperl,{"SEN",0x68,0x06},
                                parse_hex("041389E20100023B0000"));
    numeric(water,"total_m3",123.529);
    bool mismatch=false;
    try {
      (void)decode_standard_clear(iperl,{"HYD",0xFE,0x06},
                                  parse_hex("041389E20100023B0000"));
    } catch (const ClearRuntimeError &) { mismatch=true; }
    verify(mismatch,"incorrect MVT silently accepted");

    const auto mk4a=load_driver(upstream("mkradio4a"));
    auto warm=decode_standard_clear(mk4a,{"TCH",0x95,0x37},
                                    parse_hex("4315A10000426CBF2C"));
    numeric(warm,"target_m3",16.1);
    verify(warm.dates.at("target")=="2021-12-31","wrong upstream warm target_date");
    auto cold=decode_standard_clear(mk4a,{"HYD",0xFE,0x06},
                                    parse_hex("4315F00200426CBF2C"));
    numeric(cold,"target_m3",75.2);
    verify(cold.dates.at("target")=="2021-12-31","wrong upstream cold target_date");

    // Independent transactions: no numerical/date state should persist.
    const auto partial=decode_standard_clear(mk4a,{"TCH",0x95,0x37},
                                             parse_hex("4315A10000"));
    numeric(partial,"target_m3",16.1);
    verify(partial.dates.empty(),"stale OMS date reused");

    bool rejected=false;
    try {
      (void)decode_standard_clear(mk4a,{"TCH",0x95,0x37},
                                  parse_hex("4315A10000426CFF2F"));
    } catch (const DvError &) { rejected=true; }
    verify(rejected,"invalid calendar data accepted");

    std::cout<<"CLEAR_RUNTIME_OK iperl 7.704/123.529, mkradio4a 16.1/75.2, OMS date\n";
    std::cout<<"No RF, AES, full status or entire upstream parity claimed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr<<"FAIL: "<<error.what()<<"\n";
    return 1;
  }
}
