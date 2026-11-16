#include "xmq_ixml_subset.h"
#include "xmq_dv_subset.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

static void require(bool condition, const char *why) {
  if (!condition) throw std::runtime_error(why);
}
static std::string upstream(const std::string &name) {
  std::ifstream f("experimental/xmq-runtime-stage-a/upstream/src/"+name+".xmq");
  if (!f) throw std::runtime_error("upstream driver not present");
  return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};
}
static const xmq_runtime::IxmlRecord &record(
    const std::vector<xmq_runtime::IxmlRecord> &records,
    const std::string &difvifkey) {
  for (const auto &r:records) if (r.difvifkey==difvifkey) return r;
  throw std::runtime_error("IXML DV mapping missing");
}
static bool fails(const xmq_runtime::Driver &driver,
                  const std::string &payload) {
  try {
    (void)xmq_runtime::decode_ixml_from_driver(driver,
                                               xmq_runtime::parse_hex(payload));
  } catch (const xmq_runtime::IxmlError &) { return true; }
  return false;
}
int main() {
  using namespace xmq_runtime;
  try {
    const auto mk=load_driver(upstream("mkradio4"));
    // Original clear payload from mkradio4.xmq: previously known
    // target 0.1 m3 / total 0.4 m3. Here verify ONLY IXML raw records.
    const auto a=decode_ixml_from_driver(mk,parse_hex(
        "06702901006017030004000300000000000000000000000000000000000000000000"));
    require(a.size()==2,"mkradio4 must synthesize two DV captures");
    require(record(a,"4215").unsigned_raw==1,"mkradio4 Prev IXML offset");
    require(record(a,"0215").unsigned_raw==3,"mkradio4 Curr IXML offset");
    require(fails(mk,"06702901006017"),"truncated mkradio4 payload accepted");

    const auto hca=load_driver(upstream("fhkvdataiii"));
    // Upstream fhkvdataiii.xmq version 94 plaintext fixture.
    const auto b=decode_ixml_from_driver(hca,parse_hex(
        "0F9F292D005024040011BD08380904000000070000000000000000000000000001000000000003140E"));
    require(b.size()==6,"fhkvdataiii 0F should produce 6 DV captures");
    require(record(b,"02FD3A").unsigned_raw==0x299F,"previous billing raw date");
    require(record(b,"426E").unsigned_raw==0x002D,"previous HCA");
    require(record(b,"42FD3A").unsigned_raw==0x2450,"current billing raw date");
    require(record(b,"026E").unsigned_raw==0x0004,"current HCA");
    require(record(b,"0265").data.size()==2,"room temperature raw capture");
    require(record(b,"025D").data.size()==2,"radiator temperature raw capture");
    require(fails(hca,"029F292D005024040011BD083809"),"unknown HCA marker accepted");

    // A tiny grammar unrelated to either meter: the byte assignment
    // and synthetic DV key come entirely from the supplied XMQ text.
    const auto generic=IxmlSubset(
        "decode = Frame. Frame = 'AA', Payload, byte*. "
        "Payload = word, @DV_custom. DV_custom>dvk = +'0215'.");
    const auto c=generic.decode(parse_hex("AA3412FFFF"));
    require(c.size()==1 && c[0].unsigned_raw==0x1234 &&
            c[0].difvifkey=="0215","generic IXML rule execution");
    bool unsupported=false;
    try { (void)IxmlSubset("decode = Frame. Frame = unsupported.").decode(parse_hex("01")); }
    catch (const IxmlError &) { unsupported=true; }
    require(unsupported,"unsupported grammar silently accepted");
    std::cout<<"IXML_SUBSET_OK: original mkradio4 and fhkvdataiii mappings\n";
    std::cout<<"Does not establish unit/date conversion or complete wmbusmeters parity\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr<<"FAIL: "<<e.what()<<"\n";
    return 1;
  }
}
