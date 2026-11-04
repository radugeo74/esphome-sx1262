#include "components/wmbus_native_sx126x/native_ell_integrity.h"
#include <cassert>
#include <cstdint>
#include <vector>
using namespace wmbus_pure::identity;
static std::vector<uint8_t> make_ell(uint8_t ci,uint8_t mode) {
  // Synthetic converter DLL. No user IDs, keys or real telegrams.
  std::vector<uint8_t> f{
    0,0x44,0x68,0x50,0x36,0x49,0x46,0x00,0x95,0x37,ci,
    0x00,0x01 // CC, ACC
  };
  if (ci==0x8E || ci==0x8F)
    f.insert(f.end(),{0x24,0x23,0x25,0x38,0x95,0x66,0xFE,0x06});
  if (ci==0x8D || ci==0x8F) {
    const uint32_t sn=static_cast<uint32_t>(mode)<<29;
    for (unsigned i=0;i<4;++i) f.push_back(static_cast<uint8_t>(sn>>(8*i)));
    f.insert(f.end(),{0,0}); // ELL payload CRC
  }
  const size_t payload=f.size();
  f.insert(f.end(),{0x02,0x13,0x34,0x12});
  if (ci==0x8D || ci==0x8F) {
    const uint16_t crc=wmbus_pure::crc16_en13757(f.data()+payload,f.size()-payload);
    f[payload-2]=static_cast<uint8_t>(crc);
    f[payload-1]=static_cast<uint8_t>(crc>>8);
  }
  f[0]=static_cast<uint8_t>(f.size()-1);
  return f;
}
int main() {
  for (uint8_t ci : {uint8_t{0x8D},uint8_t{0x8F}}) {
    auto frame=make_ell(ci,0);
    const auto parsed=inspect(frame.data(),frame.size());
    assert(parsed.status==Status::Ok && parsed.has_ell_session);
    auto check=inspect_ell_payload(frame.data(),frame.size(),parsed);
    assert(check.status==EllIntegrity::Valid);
    assert(check.expected==check.calculated);
    frame.back()^=1;
    check=inspect_ell_payload(frame.data(),frame.size(),
                              inspect(frame.data(),frame.size()));
    assert(check.status==EllIntegrity::BadCrc);
    frame.pop_back();
    frame[0]=static_cast<uint8_t>(frame.size()-1);
    check=inspect_ell_payload(frame.data(),frame.size(),
                              inspect(frame.data(),frame.size()));
    assert(check.status==EllIntegrity::BadCrc);
  }
  for (uint8_t ci : {uint8_t{0x8C},uint8_t{0x8E}}) {
    auto frame=make_ell(ci,0);
    auto check=inspect_ell_payload(frame.data(),frame.size(),
                                   inspect(frame.data(),frame.size()));
    assert(check.status==EllIntegrity::NoPayloadCrc);
  }
  for (uint8_t mode : {uint8_t{1},uint8_t{2},uint8_t{7}}) {
    auto frame=make_ell(0x8F,mode);
    auto check=inspect_ell_payload(frame.data(),frame.size(),
                                   inspect(frame.data(),frame.size()));
    assert(check.status==EllIntegrity::EncryptedOrUnknown);
  }
  const auto good=make_ell(0x8D,0);
  auto parsed=inspect(good.data(),good.size());
  assert(inspect_ell_payload(nullptr,0,parsed).status==EllIntegrity::Unsupported);
  parsed.application_offset=good.size()+1;
  assert(inspect_ell_payload(good.data(),good.size(),parsed).status==
         EllIntegrity::Truncated);
  auto unsupported=make_ell(0x8C,0);
  unsupported[10]=0x86;
  const auto unknown=inspect(unsupported.data(),unsupported.size());
  assert(inspect_ell_payload(unsupported.data(),unsupported.size(),unknown).status==
         EllIntegrity::Unsupported);
  return 0;
}
