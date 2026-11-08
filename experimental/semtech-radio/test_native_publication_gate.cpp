#include "components/wmbus_native_sx126x/native_publication_gate.h"
#include <cassert>
#include <cstdint>
#include <vector>
using wmbus_pure::publication::Decision;
using wmbus_pure::publication::inspect;
static std::vector<uint8_t> make(uint8_t ci) {
  // Entirely synthetic DLL (manufacturer TCH, ID 12345678).
  std::vector<uint8_t> v{0,0x44,0x68,0x50,0x78,0x56,0x34,0x12,0x95,0x62,ci};
  if (ci==0x72)
    v.insert(v.end(),{0x78,0x56,0x34,0x12,0x68,0x50,0x95,0x62,
                      0x01,0x00,0x00,0x00});
  else if (ci==0x7A)
    v.insert(v.end(),{0x01,0x00,0x00,0x00});
  else v.insert(v.end(),{0x02,0x13,0x01,0x00});
  v[0]=static_cast<uint8_t>(v.size()-1);
  return v;
}
int main() {
  constexpr uint32_t id=12345678;
  for (const uint8_t ci : {uint8_t{0xA0},uint8_t{0xA2}}) {
    const auto f=make(ci);
    assert(inspect(f.data(),f.size(),id)==Decision::Allowed);
    assert(inspect(f.data(),f.size(),id+1)==Decision::InvalidFrame);
  }
  for (const uint8_t ci : {uint8_t{0x72},uint8_t{0x7A}}) {
    auto f=make(ci);
    const size_t mode_at=ci==0x72?22:14;
    const size_t blocks_at=ci==0x72?21:13;
    // Valid Mode-0: M=N=0. Preserve this accepted clear transport.
    assert(inspect(f.data(),f.size(),id)==Decision::Allowed);
    // Invalid Mode-0: nonzero encrypted block count MUST fail closed.
    f[blocks_at]=0x20;
    assert(inspect(f.data(),f.size(),id)==Decision::UnverifiedSecurity);
    f[blocks_at]=0;
    // The former 4-bit mask treated 0x1000 (M=16) as plain Mode-0.
    for (const uint8_t mode : {uint8_t{4}, uint8_t{5}, uint8_t{7},
                               uint8_t{10}, uint8_t{13}, uint8_t{15},
                               uint8_t{16}, uint8_t{31}}) {
      f[mode_at]=mode;
      assert(inspect(f.data(),f.size(),id)==Decision::UnverifiedSecurity);
    }
    f[mode_at]=0;
    // M=0,N=0 remains clear with the OMS C/R/H message flags set.
    f[blocks_at]=0x0F;
    assert(inspect(f.data(),f.size(),id)==Decision::Allowed);
    f[blocks_at]=0;
    f[0]++;
    assert(inspect(f.data(),f.size(),id)==Decision::InvalidFrame);
    f[0]--;
    f.pop_back();
    f[0]=static_cast<uint8_t>(f.size()-1);
    assert(inspect(f.data(),f.size(),id)!=Decision::Allowed);
  }
  auto long_tpl=make(0x72);
  // Same DLL ID but another TPL manufacturer or device medium: reject.
  long_tpl[15]=0x24; long_tpl[16]=0x23;
  assert(inspect(long_tpl.data(),long_tpl.size(),id)==Decision::ConflictingIdentity);
  long_tpl=make(0x72);
  long_tpl[11]=0x77; // Different physical TPL ID via converter.
  assert(inspect(long_tpl.data(),long_tpl.size(),id)==Decision::ConflictingIdentity);
  auto unsupported=make(0x8F);
  assert(inspect(unsupported.data(),unsupported.size(),id)==Decision::UnsupportedTransport);
  assert(inspect(nullptr,0,id)==Decision::InvalidFrame);
  return 0;
}
