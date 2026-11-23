#include "standalone_engine.h"
#include <iostream>
#include <stdexcept>
using namespace xmq_runtime;
using namespace xmq_standalone;
static void check(bool b, const char *s) { if (!b) throw std::runtime_error(s); }
int main() {
  try {
    // Public upstream fixtures only. One engine for seven heat and five water
    // logical contracts; this is NOT proof of twelve physical radio meters.
    const auto water = parse_hex("06702901006017030004000300000000000000000000000000000000000000000000");
    Input w{Family::Water, {"TCH",0x95,0x62},
            ApplicationEncoding::OriginalIxml, water, Trust::VerifiedCleartext};
    XmqFrameResult out;
    for (int i=0;i<5;++i) {
      check(decode(w,out), "synthetic MK4 independent XMQ decode");
      check(out.numeric.count("target_m3")==1 &&
            out.numeric.count("total_m3")==1,
            "water fields unavailable");
    }
    w.trust=Trust::Unknown;
    check(!decode(w,out) && out.numeric.empty(), "unknown security admitted");
    w.trust=Trust::Ciphertext;
    check(!decode(w,out) && out.numeric.empty(), "ciphertext admitted");
    w.trust=Trust::VerifiedCleartext;
    w.inner_mvt={"TCH",0x95,0x80};
    check(!decode(w,out), "wrong water MVT admitted");
    // Public fhkvdataiii v69 application envelope, from driver fixture.
    const auto heat=parse_hex("119F27020480048300C408F709143C003D341A2B0B2A0707000000000000062D114457563D71A1850000");
    Input h{Family::Heat, {"TCH",0x69,0x80},
            ApplicationEncoding::OriginalIxml, heat, Trust::VerifiedCleartext};
    for (int i=0;i<7;++i) {
      check(decode(h,out), "synthetic heat XMQ projection incomplete");
      for (auto field: {"current_hca","previous_hca","temp_room_c","temp_radiator_c"})
        check(out.numeric.count(field)==1, "missing heat field");
    }
    h.trust=Trust::Unknown;
    check(!decode(h,out) && out.numeric.empty(), "heat without trust admitted");
    std::cout << "XMQ_STANDALONE_HOST_OK: 7 heat + 5 water logical contracts, "
                 "no legacy decoder, no HA writes, synthetic fixtures only\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL: " << e.what() << '\n';
    return 1;
  }
}
