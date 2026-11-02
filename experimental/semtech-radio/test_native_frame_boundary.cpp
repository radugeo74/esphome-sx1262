#include "../../components/wmbus_native_sx126x/native_frame_boundary.h"
#include <cassert>
#include <cstdint>
using namespace esphome::wmbus_native_sx126x::boundary;
int main() {
  const uint8_t c1b[] = {0x0B,0x44,0x68,0x50,0x01,0x00,0x00,0x31,0x95,0x37,0x00,0x00};
  const auto valid = convert(Mode::C1B, c1b, sizeof(c1b));
  assert(valid.status == Status::Framed);
  assert(valid.decoded_dll_with_crc.size() == sizeof(c1b));
  assert(valid.decoded_dll_with_crc[0] == 0x0B);
  assert(convert(Mode::C1B, c1b, 8).status == Status::Incomplete);
  uint8_t c1a[21] = {0x10};
  assert(convert(Mode::C1A, c1a, sizeof(c1a)).status == Status::Framed);
  assert(convert(Mode::C1A, c1a, 5).status == Status::Incomplete);
  const uint8_t t1coded[] = {0x59,0x95,0x99};
  // 0x59 0x95 0x99 encodes 0x05 0x05 using canonical 3-of-6 symbols.
  // Do not assume it is a complete telegram or has valid DLL CRC.
  assert(convert(Mode::T1, t1coded, sizeof(t1coded), 2).status == Status::Framed);
  assert(convert(Mode::T1, t1coded, 1, 2).status == Status::Incomplete);
  assert(convert(Mode::T1, nullptr, 0, 2).status == Status::InvalidInput);
  return 0;
}
