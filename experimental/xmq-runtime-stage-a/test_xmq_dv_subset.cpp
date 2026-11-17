#include "xmq_dv_subset.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

static void require(bool yes, const char *message) {
  if (!yes) throw std::runtime_error(message);
}
static bool rejects(const std::string &hex) {
  try {
    (void)xmq_runtime::decode_dv_subset(xmq_runtime::parse_hex(hex));
  } catch (const xmq_runtime::DvError &) { return true; }
  return false;
}
int main() {
  using namespace xmq_runtime;
  try {
    // Entire strings after '_' are original *plaintext application payloads*
    // in the upstream iperl and mkradio4a XMQ test fixtures.
    auto iperl = decode_dv_subset(parse_hex("2F2F0413181E0000023B00002F2F2F2F"));
    require(iperl.size()==2, "iperl record count");
    require(iperl[0].meta.difvifkey=="0413" &&
            iperl[0].meta.vif_range=="Volume" &&
            iperl[0].meta.storage_nr==0 &&
            iperl[0].unit=="m3" &&
            std::abs(iperl[0].value-7.704)<1e-9,
            "iperl total 7.704m3");
    require(iperl[1].meta.vif_range=="VolumeFlow" &&
            std::abs(iperl[1].value)<1e-9,
            "iperl zero max flow");
    auto iperl2=decode_dv_subset(parse_hex("041389E20100023B0000"));
    require(iperl2.size()==2 &&
            std::abs(iperl2[0].value-123.529)<1e-9,
            "iperl unencrypted 123.529m3");

    auto mk4a=decode_dv_subset(parse_hex("4315A10000426CBF2C"));
    require(mk4a.size()==2 && mk4a[0].meta.storage_nr==1 &&
            mk4a[0].meta.difvifkey=="4315" &&
            std::abs(mk4a[0].value-16.1)<1e-9,
            "mkradio4a target 16.1m3");
    auto mk4a2=decode_dv_subset(parse_hex("4315F00200426CBF2C"));
    require(mk4a2.size()==2 &&
            std::abs(mk4a2[0].value-75.2)<1e-9,
            "mkradio4a cold target 75.2m3");

    require(rejects("0413181E"),"truncated value rejected");
    require(rejects("0413GG"),"invalid hex rejected");
    require(rejects("0413FF"),"partial 32-bit rejected");
    require(rejects("8201"),"truncated DIFE rejected");
    std::cout<<"DV_SUBSET_OK iperl 7.704/123.529m3 mkradio4a 16.1/75.2m3\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr<<"FAIL "<<e.what()<<"\n";
    return 1;
  }
}
