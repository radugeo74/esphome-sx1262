#include "../../components/wmbus_native_sx126x/c1_block_diagnostics.h"
#include <cassert>
#include <cstdint>
#include <vector>
namespace cd = esphome::wmbus_native_sx126x::c1diagnostic;
using wmbus_pure::CrcFrameFormat;
static void append_crc(std::vector<uint8_t> &out, size_t offset, size_t count) {
  const uint16_t crc = wmbus_pure::crc16_en13757(out.data() + offset, count);
  out.push_back(static_cast<uint8_t>(crc >> 8));
  out.push_back(static_cast<uint8_t>(crc));
}
int main() {
  // Synthetic format A L=0x24: block 1 (10 bytes), block 2 (16), block 3 (11).
  std::vector<uint8_t> a{0x24,0x44,0x68,0x50,0x11,0x22,0x33,0x44,0x55,0x66};
  append_crc(a,0,10);
  for (uint8_t i=0;i<16;++i) a.push_back(i);
  append_crc(a,12,16);
  for (uint8_t i=0;i<11;++i) a.push_back(i+16);
  append_crc(a,30,11);
  auto r = cd::inspect(a.data(),a.size(),CrcFrameFormat::A);
  assert(r.failure==cd::Failure::None);
  a[18] ^= 1;
  r=cd::inspect(a.data(),a.size(),CrcFrameFormat::A);
  assert(r.failure==cd::Failure::BadCrc && r.block==2 && r.offset==12);
  a[18] ^= 1;
  a[34] ^= 1;
  r=cd::inspect(a.data(),a.size(),CrcFrameFormat::A);
  assert(r.failure==cd::Failure::BadCrc && r.block==3 && r.offset==30);
  assert(cd::inspect(a.data(),a.size()-1,CrcFrameFormat::A).failure==cd::Failure::LengthMismatch);
  // Synthetic format B short frame, last two bytes CRC.
  std::vector<uint8_t> b{0x0B,0x44,0x68,0x50,1,2,3,4,5,6};
  append_crc(b,0,10);
  assert(cd::inspect(b.data(),b.size(),CrcFrameFormat::B).failure==cd::Failure::None);
  b[5] ^= 1;
  assert(cd::inspect(b.data(),b.size(),CrcFrameFormat::B).block==1);
  return 0;
}
