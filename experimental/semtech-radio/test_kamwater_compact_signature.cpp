#include "components/wmbus_native_sx126x/kamwater_compact_signature.h"
#include "components/wmbus_native_sx126x/kamwater_compact_admission.h"
#include <cassert>
#include <cstdint>
int main() {
  using namespace xmq_portable::kamwater;
  // Exact upstream XMQ declared format signatures, calculated independently
  // against the published CRC16 EN13757 algorithm.
  static constexpr uint16_t expected[]={
    0xA8ED,0xC412,0x0905,0x36D4,0x61EB,
    0xD2F7,0xDD34,0x7C0E,0xA9F3
  };
  size_t count=0;
  const signature::Format *table=signature::formats(count);
  assert(count==9);
  for(size_t i=0;i<count;++i) {
    assert(signature::crc16_en13757(table[i].bytes,table[i].size)==expected[i]);
    int profile=-1;
    assert(signature::identify(expected[i],profile)==signature::Result::Matched);
    assert(profile==static_cast<int>(i));
  }
  // First observed compact example is EDA8 little-endian = 0xA8ED.
  const uint8_t observed[]={0xED,0xA8};
  const uint16_t sig=static_cast<uint16_t>(observed[0]) |
                     (static_cast<uint16_t>(observed[1])<<8);
  int profile=-1;
  assert(signature::identify(sig,profile)==signature::Result::Matched);
  assert(profile==0);
  // Signature matching MUST NOT turn into authorization.
  admission::Evidence e{};
  e.profile_index=profile;
  e.compact_format_signature_verified=true;
  assert(admission::check(e)==admission::Decision::MissingIdentity);
  e.manufacturer_and_model_verified=true;
  assert(admission::check(e)==admission::Decision::UnverifiedSecurity);
  e.ell_security=admission::SecurityProof::Encrypted;
  assert(admission::check(e)==admission::Decision::UnverifiedSecurity);
  // Unknown signature cannot select a guessed format.
  assert(signature::identify(0xFFFF,profile)==signature::Result::Unknown);
  assert(profile==-1);
  // Corrupt bit in observed official format signature must fail closed.
  assert(signature::identify(static_cast<uint16_t>(sig^1),profile)==
         signature::Result::Unknown);
  assert(profile==-1);
  return 0;
}
