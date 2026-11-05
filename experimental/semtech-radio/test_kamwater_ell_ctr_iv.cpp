#include "host_only/kamwater_ell_ctr_iv.h"
#include <cassert>
#include <cstdint>
int main() {
  using namespace xmq_portable::kamwater::ell_ctr;
  const uint8_t m[2]={0x2D,0x2C};
  const uint8_t a[6]={0x78,0x56,0x34,0x12,0x01,0x07};
  const uint8_t ell[]={0x8D,0x32,0xE1,0x03,0x00,0x00,0x20,0,0};
  Iv iv{};
  assert(derive_iv(m,a,ell,sizeof(ell),iv)==Status::Ok);
  const uint8_t expected[16]={
    0x2D,0x2C,0x78,0x56,0x34,0x12,0x01,0x07,
    0x20,0x03,0x00,0x00,0x20,0,0,0
  };
  for(unsigned i=0;i<16;++i) assert(iv.bytes[i]==expected[i]);
  // CC hop/repeated fields (0x10/0x02) have no impact on the IV.
  auto changed_cc[9]={0x8D,0x20,0xE1,0x03,0x00,0x00,0x20,0,0};
  assert(derive_iv(m,a,changed_cc,sizeof(changed_cc),iv)==Status::Ok);
  for(unsigned i=0;i<16;++i) assert(iv.bytes[i]==expected[i]);
  // A non-AES-CTR session is never admitted, output is cleared.
  changed_cc[6]=0;
  assert(derive_iv(m,a,changed_cc,sizeof(changed_cc),iv)==Status::NotAesCtr);
  for(auto byte:iv.bytes) assert(byte==0);
  changed_cc[6]=0x40; // reserved ELL mode
  assert(derive_iv(m,a,changed_cc,sizeof(changed_cc),iv)==Status::NotAesCtr);
  changed_cc[0]=0x8F;
  assert(derive_iv(m,a,changed_cc,sizeof(changed_cc),iv)==Status::UnsupportedCi);
  assert(derive_iv(m,a,ell,6,iv)==Status::InvalidInput);
  assert(derive_iv(nullptr,a,ell,sizeof(ell),iv)==Status::InvalidInput);
  return 0;
}
