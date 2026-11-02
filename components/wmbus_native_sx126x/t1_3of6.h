#pragma once
// Standalone wM-Bus T1 3-of-6 PHY decoder.
// Input is raw coded bytes AFTER the hardware sync word, not a decoded DLL.
// Side-effect-free and radio independent; no meter decoding/publication here.
#include <cstddef>
#include <cstdint>
#include <vector>
namespace esphome {
namespace wmbus_native_sx126x {
namespace t1 {
enum class DecodeStatus : uint8_t { Ok, Empty, BadSymbol, Truncated, Oversize };
inline DecodeStatus decode(const uint8_t *input, size_t size,
                           size_t decoded_length, std::vector<uint8_t> &out) {
  out.clear();
  if (!input || !size || !decoded_length) return DecodeStatus::Empty;
  if (decoded_length > 255) return DecodeStatus::Oversize;
  // 2 symbols per byte, 6 bits per symbol. Check before any read.
  const size_t needed = (decoded_length * 12 + 7) / 8;
  if (size < needed) return DecodeStatus::Truncated;
  static constexpr uint8_t symbols[16] = {
      0x16,0x0D,0x0E,0x0B,0x1C,0x19,0x1A,0x13,
      0x2C,0x25,0x26,0x23,0x34,0x31,0x32,0x29
  };
  out.reserve(decoded_length);
  for (size_t n = 0; n < decoded_length * 2; ++n) {
    const size_t bit = n * 6;
    uint8_t code = 0;
    for (unsigned i = 0; i < 6; ++i) {
      const size_t at = bit + i;
      code = static_cast<uint8_t>((code << 1) |
          ((input[at / 8] >> (7 - (at % 8))) & 1U));
    }
    uint8_t value = 0xFF;
    for (uint8_t i = 0; i < 16; ++i) {
      if (symbols[i] == code) { value = i; break; }
    }
    if (value == 0xFF) { out.clear(); return DecodeStatus::BadSymbol; }
    if (!(n & 1)) out.push_back(static_cast<uint8_t>(value << 4));
    else out.back() |= value;
  }
  return DecodeStatus::Ok;
}
} // namespace t1
} // namespace wmbus_native_sx126x
} // namespace esphome
