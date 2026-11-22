#include "xmq_t1_clear_candidate.h"
#include "xmq_primary_readiness.h"
#include "components/xmq_shadow_probe/xmq_driver_sources.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace xmq_runtime;
static void check(bool b,const char *s) { if (!b) throw std::runtime_error(s); }

int main() {
 try {
  const auto payload=parse_hex("06702901006017030004000300000000000000000000000000000000000000000000");
  // Synthetic ID, synthetic values. No private meter frames or AES keys.
  std::vector<uint8_t> dll={0,0x44,0x68,0x50,0,0,0,0,0x95,0x62,0xA2};
  dll.insert(dll.end(),payload.begin(),payload.end());
  dll[0]=static_cast<uint8_t>(dll.size()-1);
  VerifiedApplication result;
  check(capture_t1_clear_mkradio4_candidate(dll,CandidateLinkMode::T1,true,true,result),
        "synthetic clear T1 frame refused");
  check(result.inner_mvt.manufacturer=="TCH" &&
        result.inner_mvt.version==0x95 && result.inner_mvt.type==0x62 &&
        result.payload==payload,"DLL vs application boundary mismatch");
  DriverRegistry registry({load_driver(source_mkradio4())});
  XmqFrameResult output;
  check(decode_primary_candidate(registry,result.inner_mvt,result.encoding,
        result.payload,"mkradio4",{"target_m3","total_m3"},{},output),
        "direct T1 candidate did not pass independent XMQ decode");
  check(std::fabs(output.numeric.at("target_m3").value-0.1)<0.000001 &&
        std::fabs(output.numeric.at("total_m3").value-0.4)<0.000001,
        "incorrect synthetic MK4 volumes");
  auto rejects=[&](std::vector<uint8_t> candidate,CandidateLinkMode mode,
                   bool crc_verified, bool clear_verified=true) {
    VerifiedApplication old=result;
    return !capture_t1_clear_mkradio4_candidate(candidate,mode,crc_verified,
                                              clear_verified,old) &&
           old.payload.empty() && old.inner_mvt.manufacturer.empty();
  };
  check(rejects(dll,CandidateLinkMode::C1,true),"C1 admitted without AES");
  check(rejects(dll,CandidateLinkMode::C1,true,false),
        "encrypted C1 admitted without AES");
  check(rejects(dll,CandidateLinkMode::Unknown,true),"unknown mode admitted");
  check(rejects(dll,CandidateLinkMode::T1,false),"CRC-unverified admitted");
  check(rejects(dll,CandidateLinkMode::T1,true,false),
        "security-unverified application admitted");
  auto bad=dll; bad[0]--;check(rejects(bad,CandidateLinkMode::T1,true),"bad L admitted");
  bad=dll;bad[1]=0x53;check(rejects(bad,CandidateLinkMode::T1,true),"wrong C admitted");
  bad=dll;bad[2]=0x00;check(rejects(bad,CandidateLinkMode::T1,true),"wrong maker admitted");
  bad=dll;bad[8]=0x94;check(rejects(bad,CandidateLinkMode::T1,true),"wrong version admitted");
  bad=dll;bad[9]=0x80;check(rejects(bad,CandidateLinkMode::T1,true),"wrong media admitted");
  bad=dll;bad[10]=0x7A;check(rejects(bad,CandidateLinkMode::T1,true),"wrong CI admitted");
  bad=dll;bad[11]=0xFF;check(rejects(bad,CandidateLinkMode::T1,true),"unknown envelope admitted");
  bad=dll;bad.resize(10);check(rejects(bad,CandidateLinkMode::T1,true),"short frame admitted");
  // No live radio callback or setter exists in this adapter.
  std::cout<<"STAGE4_HOST_T1_CLEAR_OK synthetic T1/A candidate -> XMQ, "
             "C1/CRC/length/CI fail-closed; no HA writes\\n";
  return 0;
 }catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<"\\n";return 1;}
}
