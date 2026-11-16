#include "xmq_numeric_runtime.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

static std::string upstream(const std::string &name) {
  std::ifstream file("experimental/xmq-runtime-stage-a/upstream/src/"+name+".xmq");
  if (!file) throw std::runtime_error("upstream XMQ file missing");
  return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
}
static void require(bool yes, const char *message) {
  if (!yes) throw std::runtime_error(message);
}
static double value(const std::map<std::string,xmq_runtime::Number> &fields,
                    const std::string &name) {
  const auto it=fields.find(name);
  if (it==fields.end()) throw std::runtime_error("missing XMQ output: "+name);
  return it->second.value;
}
int main() {
  using namespace xmq_runtime;
  try {
    const Driver mk4=load_driver(upstream("mkradio4"));
    const auto clear=parse_hex(
        "06702901006017030004000300000000000000000000000000000000000000000000");
    const auto records=decode_ixml_numeric(mk4,clear);
    require(records.size()==2,"mkradio4 expected two synthetic records");
    require(records[0].meta.difvifkey=="4215" &&
            records[0].meta.storage_nr==1,"mkradio4 previous DV key");
    require(records[1].meta.difvifkey=="0215","mkradio4 current DV key");
    const auto outputs=evaluate_numeric_fields(mk4,records);
    require(std::abs(value(outputs,"prev_raw_counter")-0.1)<1e-9,
            "mkradio4 previous raw scaled");
    require(std::abs(value(outputs,"curr_raw_counter")-0.3)<1e-9,
            "mkradio4 current raw scaled");
    require(std::abs(value(outputs,"target_m3")-0.1)<1e-9,
            "upstream target_m3 must be 0.1");
    require(std::abs(value(outputs,"total_m3")-0.4)<1e-9,
            "upstream total_m3 must be 0.4");

    // Negative: a missing byte is rejected by the original IXML source.
    bool rejected=false;
    try {
      (void)decode_ixml_numeric(mk4,parse_hex("06702901006017"));
    } catch (const IxmlError &) { rejected=true; }
    require(rejected,"partial IXML payload accepted");

    std::cout<<"NUMERIC_RUNTIME_OK mkradio4 upstream target=0.1 total=0.4\n";
    std::cout<<"Limited numeric fixture only; full XMQ parity is NOT claimed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr<<"FAIL: "<<e.what()<<"\n";
    return 1;
  }
}
