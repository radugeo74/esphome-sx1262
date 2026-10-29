#include "../../../components/wmbus_radio/pure_mode5_iv.h"
#include <cassert>
#include <cstring>
using namespace wmbus_pure;
int main() {
  // On-wire address: manufacturer TCH 0x5068, BCD ID 24273067,
  // version 0x69, media 0x80. Access number 0x2A.
  const uint8_t dll[8]={0x68,0x50,0x67,0x30,0x27,0x24,0x69,0x80};
  const uint8_t expected[16]={
      0x68,0x50,0x67,0x30,0x27,0x24,0x69,0x80,
      0x2A,0x2A,0x2A,0x2A,0x2A,0x2A,0x2A,0x2A};
  uint8_t iv[16]{};
  assert(build_mode5_iv(dll,sizeof(dll),0x2A,iv,sizeof(iv))==Mode5IvStatus::Ok);
  assert(std::memcmp(iv,expected,sizeof(iv))==0);
  assert(build_mode5_iv(dll,7,0x2A,iv,sizeof(iv))==Mode5IvStatus::InvalidInput);
  assert(build_mode5_iv(nullptr,8,0x2A,iv,sizeof(iv))==Mode5IvStatus::InvalidInput);
  assert(build_mode5_iv(dll,8,0x2A,iv,15)==Mode5IvStatus::InvalidInput);
  // Preserve an explicitly selected alternative transport-layer address.
  const uint8_t tpl[8]={0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88};
  assert(build_mode5_iv(tpl,8,0,iv,16)==Mode5IvStatus::Ok);
  assert(std::memcmp(iv,tpl,8)==0);
  for(unsigned i=8;i<16;++i) assert(iv[i]==0);
}
