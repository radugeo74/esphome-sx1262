#pragma once

// Plain C++ T-mode length gate, shared between native SX127x receiver and
// host-side tests. No ESPHome, radio or hardware dependencies.
#include <cstddef>
#include <cstdint>

namespace esphome {
namespace sx127x {

inline int t1_symbol_to_nibble(uint8_t code) {
  switch (code) {
    case 0b010110: return 0x0;
    case 0b001101: return 0x1;
    case 0b001110: return 0x2;
    case 0b001011: return 0x3;
    case 0b011100: return 0x4;
    case 0b011001: return 0x5;
    case 0b011010: return 0x6;
    case 0b010011: return 0x7;
    case 0b101100: return 0x8;
    case 0b100101: return 0x9;
    case 0b100110: return 0xA;
    case 0b100011: return 0xB;
    case 0b110100: return 0xC;
    case 0b110001: return 0xD;
    case 0b110010: return 0xE;
    case 0b101001: return 0xF;
    default: return -1;
  }
}

// SX127x's 54 3D hardware sync is already removed before reading the FIFO.
// The first three FIFO bytes encode L and C as four 3-of-6 symbols.
// Return total on-air FIFO bytes including encoded DLL CRC, or 0 if invalid.
// Final data are always checked by the shared full-frame DLL CRC validator.
inline size_t t1_expected_raw_bytes(const uint8_t *prefix, size_t prefix_size, size_t max_size) {
  if (prefix == nullptr || prefix_size < 3)
    return 0;
  const uint8_t *b = prefix;
  const int l_hi = t1_symbol_to_nibble(b[0] >> 2);
  const int l_lo = t1_symbol_to_nibble(((b[0] & 3) << 4) | (b[1] >> 4));
  const int c_hi = t1_symbol_to_nibble(((b[1] & 15) << 2) | (b[2] >> 6));
  const int c_lo = t1_symbol_to_nibble(b[2] & 63);
  if (l_hi < 0 || l_lo < 0 || c_hi < 0 || c_lo < 0)
    return 0;
  const size_t l = static_cast<size_t>((l_hi << 4) | l_lo);
  if (l < 10)
    return 0;
  const size_t blocks = 1 + (l - 9 + 15) / 16;
  const size_t decoded_bytes = l + 1 + 2 * blocks;
  const size_t raw_bytes = (3 * decoded_bytes + 1) / 2;
  return raw_bytes >= 3 && raw_bytes <= max_size ? raw_bytes : 0;
}

// C1/A carries CRCs outside its L-field; C1/B includes CRCs in L.
// The returned length excludes the 54 CD / 54 3D mode bytes, which
// are already consumed by the SX127x hardware sync/classification.
inline size_t c1_expected_raw_bytes(uint8_t l, bool format_b, size_t max_size) {
  if (l < 10)
    return 0;
  const size_t blocks = 1 + (static_cast<size_t>(l) - 9 + 15) / 16;
  const size_t length = static_cast<size_t>(l) + 1 + (format_b ? 0 : 2 * blocks);
  return length >= 3 && length <= max_size ? length : 0;
}

struct WmbusRawCandidates {
  size_t t1{0};
  size_t c1{0};
};

// For combined mode the SX127x sync detector consumes just 0x54.
// Its first FIFO byte is 0x3D (T1 or C1/B) or 0xCD (C1/A).
// T1 and C1/B must be tested as *independent* candidates. Both may look
// plausible on an initial prefix. Neither is dispatched before full CRC.
inline WmbusRawCandidates wmbus_raw_candidates(uint8_t marker, bool combined,
                                               const uint8_t *prefix, size_t length,
                                               size_t max_size) {
  WmbusRawCandidates candidates;
  if (prefix == nullptr || length < 3 || max_size < 5)
    return candidates;
  if (marker == 0x3D) {
    candidates.t1 = t1_expected_raw_bytes(prefix, length, max_size);
    if (combined)
      candidates.c1 = c1_expected_raw_bytes(prefix[0], true, max_size - 2);
  } else if (marker == 0xCD && combined) {
    candidates.c1 = c1_expected_raw_bytes(prefix[0], false, max_size - 2);
  }
  return candidates;
}

} // namespace sx127x
} // namespace esphome
