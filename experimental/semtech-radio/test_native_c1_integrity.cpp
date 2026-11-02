#include "../../components/wmbus_native_sx126x/c1_integrity.h"
#include <cassert>
#include <cstdint>
using namespace esphome::wmbus_native_sx126x::c1;
int main() {
  uint8_t frame[12] = {0x0B,0x44,0x68,0x50,0x01,0x00,0x00,0x31,0x95,0x37,0,0};
  const auto crc = en13757_crc(frame,10);
  frame[10] = static_cast<uint8_t>(crc >> 8);
  frame[11] = static_cast<uint8_t>(crc);
  assert(probe_format_b_crc(frame,sizeof(frame)) == Integrity::Valid);
  frame[9] ^= 1;
  assert(probe_format_b_crc(frame,sizeof(frame)) == Integrity::CrcMismatch);
  assert(probe_format_b_crc(frame,8) == Integrity::TooShort);
  frame[0] = 0x0C;
  assert(probe_format_b_crc(frame,sizeof(frame)) == Integrity::InvalidLength);
  // Synthetic CRC mismatch already covered above; no real RF captures in CI.
  return 0;
}
