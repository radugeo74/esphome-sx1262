#include "components/wmbus_native_sx126x/kamwater_verified_dll_ingress.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>
using namespace xmq_portable::kamwater;
static std::vector<uint8_t> make_frame() {
  // Artificial KAM address, not a real meter. Format B, full CRC at end.
  const uint8_t compact_tpl[]={
    0x79,0xED,0xA8,0x6A,0xB6,0x71,0x00,0x08,0x19,0x00,0x00,
    0x08,0x19,0x00,0x00,0x7F,0x13
  };
  std::vector<uint8_t> plain={
    0,0x44,0x2D,0x2C,0x78,0x56,0x34,0x12,0x01,0x07,
    0x8D,0x20,0x01,0x03,0,0,0,0,0
  };
  plain.insert(plain.end(),compact_tpl,compact_tpl+sizeof(compact_tpl));
  const uint16_t ecrc=signature::crc16_en13757(
      plain.data()+19,plain.size()-19);
  plain[17]=static_cast<uint8_t>(ecrc);
  plain[18]=static_cast<uint8_t>(ecrc>>8);
  // For short format B the L byte counts all bytes except itself.
  // The raw frame includes a trailing two-byte DLL CRC.
  plain[0]=static_cast<uint8_t>(plain.size()+1);
  const uint16_t dll=wmbus_pure::crc16_en13757(plain.data(),plain.size());
  plain.push_back(static_cast<uint8_t>(dll>>8));
  plain.push_back(static_cast<uint8_t>(dll));
  return plain;
}
static void renew_dll_crc(std::vector<uint8_t> &raw) {
  const uint16_t crc=wmbus_pure::crc16_en13757(raw.data(),raw.size()-2);
  raw[raw.size()-2]=static_cast<uint8_t>(crc>>8);
  raw.back()=static_cast<uint8_t>(crc);
}
int main() {
  using namespace xmq_portable::kamwater::ingress;
  using IngressStatus=xmq_portable::kamwater::ingress::Status;
  auto frame=make_frame();
  // Header parser's application pointer is AFTER CI, but ELL parsing
  // MUST include CI (the preceding byte at offset 10).
  wmbus_pure::CompleteFrame dll_check{};
  assert(wmbus_pure::verify_complete_frame(
      frame.data(),frame.size(),wmbus_pure::CrcFrameFormat::B,dll_check)
      ==wmbus_pure::CompleteStatus::Ok);
  assert(dll_check.header.ci==0x8D);
  assert(dll_check.header.application==dll_check.bytes.data()+11);
  assert(dll_check.bytes[10]==0x8D && dll_check.bytes[11]==0x20);
  const ExpectedMeter expected{0x2C2D,12345678u,0x01,0x07};
  Output out{};
  assert(decode_verified_dll(frame.data(),frame.size(),
                             wmbus_pure::CrcFrameFormat::B,expected,out)
         ==IngressStatus::Ok);
  assert(out.profile_index==0);
  assert(std::fabs(out.reading.total_m3-6.408)<1e-8);
  assert(out.reading.has_target && std::fabs(out.reading.target_m3-6.408)<1e-8);
  assert(out.reading.has_status && out.reading.status_bits==0x71);
  // CRC corruption never reaches application decoder.
  auto bad=frame;bad[4]^=1;
  assert(decode_verified_dll(bad.data(),bad.size(),
                             wmbus_pure::CrcFrameFormat::B,expected,out)
         ==IngressStatus::DllRejected);
  assert(out.profile_index==-1 && out.reading.total_m3==0);
  // Even after recomputing CRC, mismatched identity must be rejected.
  bad=frame;bad[4]=0x79;renew_dll_crc(bad);
  assert(decode_verified_dll(bad.data(),bad.size(),
                             wmbus_pure::CrcFrameFormat::B,expected,out)
         ==IngressStatus::IdentityMismatch);
  bad=frame;bad[2]^=1;renew_dll_crc(bad);
  assert(decode_verified_dll(bad.data(),bad.size(),
                             wmbus_pure::CrcFrameFormat::B,expected,out)
         ==IngressStatus::IdentityMismatch);
  bad=frame;bad[8]=0x02;renew_dll_crc(bad);
  assert(decode_verified_dll(bad.data(),bad.size(),
                             wmbus_pure::CrcFrameFormat::B,expected,out)
         ==IngressStatus::IdentityMismatch);
  bad=frame;bad[9]=0x08;renew_dll_crc(bad);
  assert(decode_verified_dll(bad.data(),bad.size(),
                             wmbus_pure::CrcFrameFormat::B,expected,out)
         ==IngressStatus::IdentityMismatch);
  // Identity parameters are required, never guessed from received header.
  const ExpectedMeter not_configured{};
  assert(decode_verified_dll(frame.data(),frame.size(),
                             wmbus_pure::CrcFrameFormat::B,not_configured,out)
         ==IngressStatus::IdentityMismatch);
  // Unsupported CI, after successful DLL CRC and identity verification.
  bad=frame;bad[10]=0x8C;renew_dll_crc(bad);
  assert(decode_verified_dll(bad.data(),bad.size(),
                             wmbus_pure::CrcFrameFormat::B,expected,out)
         ==IngressStatus::WrongCi);
  // Corrupt ELL CRC but keep valid DLL CRC: independent verification fails.
  bad=frame;bad[17]^=1;renew_dll_crc(bad);
  assert(decode_verified_dll(bad.data(),bad.size(),
                             wmbus_pure::CrcFrameFormat::B,expected,out)
         ==IngressStatus::ClearPipelineRejected);
  assert(out.profile_index==-1 && !out.reading.has_target);
  // AES CTR ELL mode cannot pass even when DLL is valid.
  bad=frame;bad[16]=0x20;renew_dll_crc(bad);
  assert(decode_verified_dll(bad.data(),bad.size(),
                             wmbus_pure::CrcFrameFormat::B,expected,out)
         ==IngressStatus::ClearPipelineRejected);
  // Valid DLL and ELL CRC, but invalid compact data CRC.
  bad=frame;bad[22]^=1;
  const uint16_t crc=signature::crc16_en13757(bad.data()+19,bad.size()-21);
  bad[17]=static_cast<uint8_t>(crc);
  bad[18]=static_cast<uint8_t>(crc>>8);
  renew_dll_crc(bad);
  assert(decode_verified_dll(bad.data(),bad.size(),
                             wmbus_pure::CrcFrameFormat::B,expected,out)
         ==IngressStatus::ClearPipelineRejected);
  assert(out.profile_index==-1 && out.reading.total_m3==0);
  // Incomplete frame cannot slip through as verified.
  bad=frame;bad.pop_back();
  assert(decode_verified_dll(bad.data(),bad.size(),
                             wmbus_pure::CrcFrameFormat::B,expected,out)
         ==IngressStatus::DllRejected);
  return 0;
}
