#pragma once
// Read-only per-block CRC diagnosis for rejected C1 frames.
// Uses synthetic fixtures only. Never repairs or accepts a failed frame.
#include <cstddef>
#include <cstdint>
#include "pure_dll_crc.h"

namespace esphome::wmbus_native_sx126x::c1diagnostic {
enum class Failure : uint8_t { None, TooShort, LengthMismatch, BadCrc };
struct Result {
  Failure failure{Failure::TooShort};
  size_t block{0};            // 1-based CRC block number
  size_t offset{0};           // start of block within DLL (excluding C1 marker)
  uint16_t received{0};
  uint16_t calculated{0};
};
inline Result inspect(const uint8_t *raw, size_t size, wmbus_pure::CrcFrameFormat format) {
  if (raw == nullptr || size < 3) return {};
  const size_t length = raw[0];
  Result result{};
  if (format == wmbus_pure::CrcFrameFormat::A) {
    const size_t first = length < 9 ? length + 1 : 10;
    const size_t rest = length + 1 - first;
    const size_t blocks = 1 + (rest + 15) / 16;
    if (size != length + 1 + 2 * blocks) {
      result.failure = Failure::LengthMismatch;
      return result;
    }
    size_t pos = 0;
    for (size_t block = 1; pos < size; ++block) {
      const size_t data_len = pos == 0 ? first :
          ((size - pos - 2) < 16 ? size - pos - 2 : 16);
      if (pos + data_len + 2 > size) {
        result.failure = Failure::LengthMismatch;
        return result;
      }
      const uint16_t calculated = wmbus_pure::crc16_en13757(raw + pos, data_len);
      const uint16_t received = (static_cast<uint16_t>(raw[pos + data_len]) << 8) |
                                raw[pos + data_len + 1];
      if (calculated != received) {
        result.failure = Failure::BadCrc;
        result.block = block;
        result.offset = pos;
        result.received = received;
        result.calculated = calculated;
        return result;
      }
      pos += data_len + 2;
    }
  } else {
    if (size != length + 1) {
      result.failure = Failure::LengthMismatch;
      return result;
    }
    const size_t first = size <= 128 ? size - 2 : 126;
    for (size_t block = 1, pos = 0; pos < size; ++block) {
      const size_t data_len = block == 1 ? first : size - pos - 2;
      if (pos + data_len + 2 > size) {
        result.failure = Failure::LengthMismatch;
        return result;
      }
      const uint16_t calculated = wmbus_pure::crc16_en13757(raw + pos, data_len);
      const uint16_t received = (static_cast<uint16_t>(raw[pos + data_len]) << 8) |
                                raw[pos + data_len + 1];
      if (calculated != received) {
        result.failure = Failure::BadCrc;
        result.block = block;
        result.offset = pos;
        result.received = received;
        result.calculated = calculated;
        return result;
      }
      pos += data_len + 2;
    }
  }
  result.failure = Failure::None;
  return result;
}
} // namespace esphome::wmbus_native_sx126x::c1diagnostic
