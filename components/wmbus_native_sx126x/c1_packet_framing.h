#pragma once
// Convert an SX126x packet received AFTER sync bytes 54 3D or 54 CD
// to a bounded raw C1 radio frame. Structural framing only: NOT DLL CRC validation.
#include <cstddef>
#include <cstdint>
#include <vector>

namespace esphome {
namespace wmbus_native_sx126x {
namespace c1 {

enum class FrameStatus : uint8_t {
  Ok,
  Empty,
  InvalidFormat,
  Incomplete,
  TooLong,
};

inline FrameStatus frame_from_post_sync(const uint8_t *bytes, size_t size,
                                        uint8_t sync_second,
                                        std::vector<uint8_t> &out) {
  out.clear();
  if (bytes == nullptr || size == 0)
    return FrameStatus::Empty;
  if (sync_second != 0x3D && sync_second != 0xCD)
    return FrameStatus::InvalidFormat;

  const size_t l = bytes[0];
  if (l < 10)
    return FrameStatus::InvalidFormat;

  size_t body_size = 0;
  if (sync_second == 0x3D) {
    // C1 format B: L counts the bytes following the L field, including DLL CRC.
    body_size = 1 + l;
  } else {
    // C1 format A, matching the established wmbus_radio::Packet convention.
    const size_t blocks = l < 26 ? 2 : ((l - 26) / 16 + 3);
    body_size = 1 + l + 2 * blocks;
  }
  if (body_size + 2 > 255)
    return FrameStatus::TooLong;
  if (size < body_size)
    return FrameStatus::Incomplete;

  out.reserve(body_size + 2);
  out.push_back(0x54);
  out.push_back(sync_second);
  out.insert(out.end(), bytes, bytes + body_size);
  return FrameStatus::Ok;
}

}  // namespace c1
}  // namespace wmbus_native_sx126x
}  // namespace esphome
