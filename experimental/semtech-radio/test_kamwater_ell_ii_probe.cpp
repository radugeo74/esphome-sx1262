#include "components/wmbus_native_sx126x/kamwater_ell_ii_probe.h"
#include "components/wmbus_native_sx126x/kamwater_compact_admission.h"
#include <cassert>
#include <cstdint>
#include <vector>
int main() {
  using namespace xmq_portable::kamwater;
  // Synthetic ELL II clear application: CI CC ACC SN(LE) CRC16(LE)
  // followed by TPL 78 and public example's clear DIF/VIF fields.
  std::vector<uint8_t> frame={
    0x8D,0x20,0x91,0x03,0x00,0x00,0x00,0,0,0x78,
    0x02,0xFF,0x20,0x71,0x00,0x04,0x13,0x08,0x19,0x00,0x00
  };
  const uint16_t crc=signature::crc16_en13757(frame.data()+9,frame.size()-9);
  frame[7]=static_cast<uint8_t>(crc);
  frame[8]=static_cast<uint8_t>(crc>>8);
  ell::ClearPayload result{};
  assert(ell::inspect_ell_ii(frame.data(),frame.size(),result)
         ==ell::Status::ClearAndCrcValid);
  assert(result.tpl==frame.data()+9 && result.tpl_size==frame.size()-9);
  assert(result.session_number==3);
  // Corruption anywhere under the payload CRC must fail, with zero output.
  frame.back()^=0x01;
  assert(ell::inspect_ell_ii(frame.data(),frame.size(),result)
         ==ell::Status::BadPayloadCrc);
  assert(result.tpl==nullptr && result.tpl_size==0);
  frame.back()^=0x01;
  frame[7]^=0x01;
  assert(ell::inspect_ell_ii(frame.data(),frame.size(),result)
         ==ell::Status::BadPayloadCrc);
  frame[7]^=0x01;
  // ELL AES-CTR mode 1 explicitly rejected even with a valid plaintext CRC.
  frame[6]=0x20;
  assert(ell::inspect_ell_ii(frame.data(),frame.size(),result)
         ==ell::Status::Encrypted);
  assert(result.tpl==nullptr);
  frame[6]=0x40;
  assert(ell::inspect_ell_ii(frame.data(),frame.size(),result)
         ==ell::Status::ReservedSecurity);
  frame[6]=0;
  auto wrong_ci=frame; wrong_ci[0]=0x8F;
  assert(ell::inspect_ell_ii(wrong_ci.data(),wrong_ci.size(),result)
         ==ell::Status::UnsupportedCi);
  auto truncated=frame; truncated.resize(10);
  assert(ell::inspect_ell_ii(truncated.data(),truncated.size(),result)
         ==ell::Status::Invalid);
  // CRC valid but unsupported TPL is still not admitted.
  auto wrong_tpl=frame;
  wrong_tpl[9]=0x72;
  const uint16_t crc2=signature::crc16_en13757(
      wrong_tpl.data()+9,wrong_tpl.size()-9);
  wrong_tpl[7]=static_cast<uint8_t>(crc2);
  wrong_tpl[8]=static_cast<uint8_t>(crc2>>8);
  assert(ell::inspect_ell_ii(wrong_tpl.data(),wrong_tpl.size(),result)
         ==ell::Status::UnsupportedTpl);
  // The public raw Kamstrup header is AES CTR (SN top 3 bits = 1).
  const uint8_t upstream_header[]={
    0x8D,0x20,0x91,0xD3,0x7C,0xAC,0x21,0x57,0x6C,0x78,0x02,0xFF
  };
  assert(ell::inspect_ell_ii(upstream_header,sizeof(upstream_header),result)
         ==ell::Status::Encrypted);
  // ELL CRC success does NOT prove compact format or device identity.
  admission::Evidence evidence{};
  evidence.ell_security=admission::SecurityProof::VerifiedClearApplication;
  assert(admission::check(evidence)==admission::Decision::MissingIdentity);
  return 0;
}
