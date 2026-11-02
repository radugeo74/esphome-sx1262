#include "../../components/wmbus_native_sx126x/c1_packet_framing.h"
#include <cassert>
#include <cstdint>
#include <vector>

using namespace esphome::wmbus_native_sx126x::c1;
int main() {
  // Synthetic format B telegram: 55 post-sync bytes, no real meter data.
  uint8_t b[55] = {};
  b[0] = 0x36;
  b[1] = 0x44;
  b[2] = 0x68;
  b[3] = 0x50;
  b[4] = 0x01; b[5] = 0x00; b[6] = 0x00; b[7] = 0x31;
  b[8] = 0x95; b[9] = 0x37; b[10] = 0x72;
  b[54] = 0x51; // sentinel for frame-boundary assertion, not a CRC
  static_assert(sizeof(b) == 55, "synthetic packet length");
  std::vector<uint8_t> raw;
  assert(frame_from_post_sync(b, sizeof(b), 0x3D, raw) == FrameStatus::Ok);
  assert(raw.size() == 57);
  assert(raw[0] == 0x54 && raw[1] == 0x3D);
  assert(raw[2] == 0x36 && raw.back() == 0x51);
  uint8_t padded[255] = {};
  for (size_t i = 0; i < sizeof(b); ++i) padded[i] = b[i];
  assert(frame_from_post_sync(padded, sizeof(padded), 0x3D, raw) == FrameStatus::Ok);
  assert(raw.size() == 57);
  assert(frame_from_post_sync(b, 12, 0x3D, raw) == FrameStatus::Incomplete);
  assert(raw.empty());
  assert(frame_from_post_sync(b, sizeof(b), 0x00, raw) == FrameStatus::InvalidFormat);
  assert(frame_from_post_sync(nullptr, 0, 0x3D, raw) == FrameStatus::Empty);
  uint8_t bad[] = {0x08,0x00};
  assert(frame_from_post_sync(bad, sizeof(bad), 0x3D, raw) == FrameStatus::InvalidFormat);
  // Format A: L=0x10 => 1+16+4=21 bytes following sync.
  uint8_t a[21] = {0x10};
  assert(frame_from_post_sync(a, sizeof(a), 0xCD, raw) == FrameStatus::Ok);
  assert(raw.size() == 23);
  return 0;
}
