#pragma once
// Standalone EN 13757 DLL CRC checker/stripper for decoded format A/B frames.
// No driver or legacy wmbus_common dependencies. Does not mutate on failure.
#include <cstddef>
#include <cstdint>
#include <vector>
namespace wmbus_pure {
enum class CrcFrameFormat : uint8_t { A, B };
enum class CrcStatus : uint8_t { Ok, TooShort, LengthMismatch, BadCrc };
inline uint16_t crc16_en13757(const uint8_t *data, size_t size) {
  uint16_t crc = 0;
  for (size_t i = 0; i < size; ++i) {
    uint8_t value = data[i];
    for (unsigned bit = 0; bit < 8; ++bit) {
      const bool differ = ((crc & 0x8000u) != 0) != ((value & 0x80u) != 0);
      crc = static_cast<uint16_t>((crc << 1) ^ (differ ? 0x3D65u : 0));
      value <<= 1;
    }
  }
  return static_cast<uint16_t>(~crc);
}
inline bool crc_block_valid(const uint8_t *data, size_t bytes) {
  const uint16_t received = (static_cast<uint16_t>(data[bytes]) << 8) |
                            data[bytes + 1];
  return crc16_en13757(data, bytes) == received;
}
inline CrcStatus strip_dll_crc(const uint8_t *raw, size_t size,
                               CrcFrameFormat format, std::vector<uint8_t> &output) {
  if (!raw || size < 12) return CrcStatus::TooShort;
  const size_t length = raw[0];
  std::vector<uint8_t> staged;
  staged.reserve(size);
  if (format == CrcFrameFormat::A) {
    // L counts all bytes other than L itself and the block CRCs.
    const size_t first = length < 9 ? length + 1 : 10;
    const size_t rest = length + 1 - first;
    const size_t blocks = 1 + (rest + 15) / 16;
    if (size != length + 1 + 2 * blocks) return CrcStatus::LengthMismatch;
    size_t pos = 0;
    while (pos < size) {
      const size_t block_bytes = pos == 0 ? first :
          ((size - pos - 2) < 16 ? size - pos - 2 : 16);
      if (pos + block_bytes + 2 > size || !crc_block_valid(raw + pos, block_bytes))
        return CrcStatus::BadCrc;
      staged.insert(staged.end(), raw + pos, raw + pos + block_bytes);
      pos += block_bytes + 2;
    }
  } else {
    // Frame B: first CRC after at most 126 bytes; optional second CRC at end.
    if (size != length + 1) return CrcStatus::LengthMismatch;
    const size_t first = size <= 128 ? size - 2 : 126;
    if (!crc_block_valid(raw, first)) return CrcStatus::BadCrc;
    staged.insert(staged.end(), raw, raw + first);
    if (size > 128) {
      const size_t second_start = 128;
      const size_t second_length = size - second_start - 2;
      if (!crc_block_valid(raw + second_start, second_length))
        return CrcStatus::BadCrc;
      staged.insert(staged.end(), raw + second_start,
                    raw + second_start + second_length);
    }
  }
  if (staged.empty() || staged.size() > 256) return CrcStatus::LengthMismatch;
  staged[0] = static_cast<uint8_t>(staged.size() - 1);
  output.swap(staged);
  return CrcStatus::Ok;
}
}  // namespace wmbus_pure
