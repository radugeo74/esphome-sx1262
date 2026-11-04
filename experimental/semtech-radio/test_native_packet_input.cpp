#include "../../components/wmbus_native_sx126x/native_packet_input.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>
using namespace esphome::wmbus_native_sx126x::native_radio;

int main() {
  const uint8_t frame[] = {0x54, 0x3d, 0x36, 0x44, 0x68, 0x50};
  std::vector<uint8_t> owned{0xff};
  PacketView input{frame, sizeof(frame), -86.0f, 0.0f};
  assert(copy_validated(input, owned) == InputStatus::Ok);
  assert(owned.size() == sizeof(frame));
  assert(owned.front() == 0x54);
  assert(owned.back() == 0x50);
  assert(owned.data() != frame);
  assert(copy_validated({nullptr, 0, -86.0f, 0.0f}, owned) == InputStatus::Empty);
  assert(owned.empty());
  assert(copy_validated({frame, 256, -86.0f, 0.0f}, owned) == InputStatus::Oversize);
  assert(owned.empty());
  assert(copy_validated({frame, sizeof(frame), NAN, 0.0f}, owned) ==
         InputStatus::InvalidRssi);
  assert(owned.empty());
  return 0;
}
