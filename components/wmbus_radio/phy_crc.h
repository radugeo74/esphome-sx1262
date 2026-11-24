#pragma once
// Independent EN 13757 DLL CRC verification and CRC-stripper.
// Native T1 3-of-6 decoding remains in decode3of6.cpp; both SX126x and
// SX127x use this layer. Reject incomplete or overlong radio frames.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace esphome { namespace wmbus_radio { namespace phy {
inline uint16_t crc16(const uint8_t *data, size_t size) {
  uint16_t crc = 0;
  for (size_t i = 0; i < size; ++i) {
    uint8_t octet = data[i];
    for (unsigned bit = 0; bit != 8; ++bit) {
      const bool msb = ((crc & 0x8000u) != 0) != ((octet & 0x80u) != 0);
      crc = static_cast<uint16_t>(crc << 1);
      if (msb) crc ^= 0x3d65u;
      octet <<= 1;
    }
  }
  return static_cast<uint16_t>(~crc);
}
inline bool match_crc(const std::vector<uint8_t> &frame, size_t start, size_t count) {
  if (count == 0 || start > frame.size() ||
      count > frame.size() - start ||
      frame.size() - start - count < 2) return false;
  const uint16_t received = (uint16_t(frame[start + count]) << 8) |
                            uint16_t(frame[start + count + 1]);
  return crc16(frame.data() + start, count) == received;
}
inline bool strip_format_a(std::vector<uint8_t> &frame) {
  if (frame.size() < 14 || frame[0] < 10) return false;
  const size_t declared = size_t(frame[0]) + 1;
  const size_t count = 1 + (size_t(frame[0]) - 9 + 15) / 16;
  if (declared + 2 * count != frame.size() || declared > 256 ||
      !match_crc(frame, 0, 10)) return false;
  std::vector<uint8_t> clear;
  clear.reserve(declared);
  clear.insert(clear.end(), frame.begin(), frame.begin() + 10);
  size_t pos = 12;
  while (pos < frame.size()) {
    const size_t remaining = frame.size() - pos;
    if (remaining < 3) return false;
    const size_t n = std::min(size_t(16), remaining - 2);
    if (!match_crc(frame, pos, n)) return false;
    clear.insert(clear.end(), frame.begin() + pos, frame.begin() + pos + n);
    pos += n + 2;
  }
  if (clear.size() != declared) return false;
  frame.swap(clear);
  return true;
}
inline bool strip_format_b(std::vector<uint8_t> &frame) {
  if (frame.size() < 12 || frame[0] < 10 ||
      frame.size() != size_t(frame[0]) + 1) return false;
  const size_t first_crc = frame.size() <= 128 ? frame.size() - 2 : 126;
  const size_t second_crc = frame.size() <= 128 ? 0 : frame.size() - 2;
  if (!match_crc(frame, 0, first_crc)) return false;
  std::vector<uint8_t> clear;
  clear.reserve(frame.size());
  clear.insert(clear.end(), frame.begin(), frame.begin() + first_crc);
  if (second_crc) {
    const size_t second_start = first_crc + 2;
    if (second_crc <= second_start ||
        !match_crc(frame, second_start, second_crc - second_start)) return false;
    clear.insert(clear.end(), frame.begin() + second_start,
                 frame.begin() + second_crc);
  }
  if (clear.empty() || clear.size() > 256) return false;
  clear[0] = static_cast<uint8_t>(clear.size() - 1);
  frame.swap(clear);
  return true;
}
}}} // namespace esphome::wmbus_radio::phy
