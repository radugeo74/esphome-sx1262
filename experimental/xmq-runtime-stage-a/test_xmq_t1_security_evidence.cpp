#include "xmq_t1_security_evidence.h"
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace xmq_runtime;
static void require(bool yes, const char *why) {
  if (!yes) throw std::runtime_error(why);
}
int main() {
  try {
    // Synthetic TCH MK4 T1/A DLL. No real meter identifier, payload or key.
    std::vector<uint8_t> frame = {
      0,0x44,0x68,0x50,0,0,0,0,0x95,0x62,0xA2,
      0x06,0x3e,0x35,0x70,0x29,0x01,0x00
    };
    frame[0]=static_cast<uint8_t>(frame.size()-1);
    const auto classify=[&](bool crc=true, bool t1=true) {
      return inspect_t1_envelope_without_legacy(frame,crc,t1);
    };
    require(classify()==T1EnvelopeEvidence::ProprietaryMk4Candidate,
            "known T1 proprietary envelope rejected");
    require(classify(false)==T1EnvelopeEvidence::Reject,
            "CRC-unverified frame accepted");
    require(classify(true,false)==T1EnvelopeEvidence::Reject,
            "non-T1 frame accepted");
    frame[10]=0x72;
    require(classify()==T1EnvelopeEvidence::UnknownSecurity,
            "standard CI incorrectly declared clear");
    frame[10]=0xA2; frame[11]=0xFF;
    require(classify()==T1EnvelopeEvidence::UnknownSecurity,
            "unknown A2 envelope incorrectly declared clear");
    frame[11]=0x06;frame[12]=0x70;frame[13]=0x29;
    require(classify()==T1EnvelopeEvidence::ProprietaryMk4Candidate,
            "second known MK4 envelope rejected");
    frame[8]=0x94;
    require(classify()==T1EnvelopeEvidence::Reject,
            "HCA MVT accepted as MK4");
    frame[8]=0x95;frame[0]--;
    require(classify()==T1EnvelopeEvidence::Reject,
            "wrong DLL L accepted");
    frame[0]++;
    frame.resize(10);
    require(classify()==T1EnvelopeEvidence::Reject,
            "short frame accepted");
    std::cout<<"STAGE5_T1_SECURITY_EVIDENCE_OK structural only; "
                "no security proof, no sensor writes\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr<<"FAIL "<<e.what()<<"\n";
    return 1;
  }
}
