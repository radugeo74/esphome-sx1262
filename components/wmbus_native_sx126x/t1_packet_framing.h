#pragma once
// Diagnostic framing of variable-length T1 post-sync 3-of-6 radio data.
// This computes the DLL length from the first decoded L field and checks
// symbol validity, but does NOT validate EN13757 CRC or publish meter data.
#include <cstddef>
#include <cstdint>
#include <vector>
#include "t1_3of6.h"

namespace esphome {
namespace wmbus_native_sx126x {
namespace t1 {
enum class FrameStatus : uint8_t {
  Ok, Empty, HeaderInvalid, Incomplete, BadSymbol, TooLong
};
// Determine exact encoded size after the first two 3-of-6 symbols arrive.
// This is a pure sizing contract for a future progressive FIFO reader.
// A complete packet still requires independent symbol, DLL CRC and header checks.
inline FrameStatus expected_encoded_size(const uint8_t *input, size_t size,
                                         size_t &required) {
  required = 0;
  if (!input || !size) return FrameStatus::Empty;
  std::vector<uint8_t> first;
  const auto status = decode(input, size, 1, first);
  if (status == DecodeStatus::Truncated) return FrameStatus::Incomplete;
  if (status == DecodeStatus::BadSymbol) return FrameStatus::BadSymbol;
  if (status != DecodeStatus::Ok) return FrameStatus::HeaderInvalid;
  const size_t l = first[0];
  if (l < 10) return FrameStatus::HeaderInvalid;
  const size_t first_block = 10;
  const size_t remaining = l + 1 - first_block;
  const size_t crc_blocks = 1 + (remaining + 15) / 16;
  const size_t decoded_length = l + 1 + 2 * crc_blocks;
  if (decoded_length > 255) return FrameStatus::TooLong;
  required = (decoded_length * 12 + 7) / 8;
  return FrameStatus::Ok;
}
inline FrameStatus frame_from_post_sync(const uint8_t *input, size_t size,
                                        std::vector<uint8_t> &dll) {
  dll.clear();
  if (!input || !size) return FrameStatus::Empty;
  size_t required = 0;
  const auto sizing = expected_encoded_size(input, size, required);
  if (sizing != FrameStatus::Ok) return sizing;
  // Encoded 3-of-6 input uses 12 bits per decoded byte.
  // Ceil(12*n/8) is injective for integral n; this inverse is exact.
  const size_t decoded_length = (required * 2) / 3;
  if (size < required) return FrameStatus::Incomplete;
  const auto result = decode(input, required, decoded_length, dll);
  if (result == DecodeStatus::BadSymbol) return FrameStatus::BadSymbol;
  if (result != DecodeStatus::Ok) return FrameStatus::HeaderInvalid;
  return FrameStatus::Ok;
}
} // namespace t1
} // namespace wmbus_native_sx126x
} // namespace esphome
