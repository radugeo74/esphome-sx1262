#pragma once
// Bounded radio packet layout inspection before CRC removal.
// Does not claim CRC validity or replace frame verification.
#include <cstddef>
#include <cstdint>

namespace wmbus_pure {
enum class RawMode : uint8_t { T1, C1 };
enum class RawFormat : uint8_t { A, B, Unknown };
enum class LayoutStatus : uint8_t { Ok, TooShort, UnsupportedFormat, InvalidLength };
struct RawLayout {
  RawMode mode;
  RawFormat format;
  uint8_t l_field;
  size_t expected_decoded_bytes;
};
inline LayoutStatus inspect_raw_layout(const uint8_t *raw, size_t size,
                                      RawMode mode, RawLayout &out) {
  out = {};
  out.mode = mode;
  if (!raw || size < 3) return LayoutStatus::TooShort;
  // C1 preamble (54 CD / 54 3D) followed by the L-field.
  // T1 requires 3-of-6 decoding first and is not guessed here.
  if (mode != RawMode::C1) return LayoutStatus::UnsupportedFormat;
  if (raw[0] != 0x54) return LayoutStatus::UnsupportedFormat;
  out.l_field = raw[2];
  if (raw[1] == 0xCD) {
    out.format = RawFormat::A;
    // First 10 data bytes after L, then 16-byte data blocks with two CRC bytes.
    const size_t payload = static_cast<size_t>(out.l_field);
    if (payload == 0) return LayoutStatus::InvalidLength;
    const size_t blocks = payload <= 10 ? 1 : 1 + (payload - 10 + 15) / 16;
    out.expected_decoded_bytes = 2 + 1 + payload + 2 * blocks;
  } else if (raw[1] == 0x3D) {
    out.format = RawFormat::B;
    out.expected_decoded_bytes = 2 + 1 + static_cast<size_t>(out.l_field);
  } else {
    return LayoutStatus::UnsupportedFormat;
  }
  return LayoutStatus::Ok;
}
}  // namespace wmbus_pure
