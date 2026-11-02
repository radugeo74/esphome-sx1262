#pragma once
// Diagnostic-only C1 format B DLL CRC probe.
// NOT an authentication or decryption primitive; never gate sensor writes with
// this until observed wire framing / CRC placement is confirmed on-device.
#include <cstddef>
#include <cstdint>

namespace esphome {
namespace wmbus_native_sx126x {
namespace c1 {
enum class Integrity : uint8_t { Valid, TooShort, InvalidLength, CrcMismatch };

inline uint16_t en13757_crc(const uint8_t *data, size_t n) {
  uint16_t crc = 0;
  for (size_t i = 0; i < n; ++i) {
    uint8_t x = data[i];
    for (int bit = 0; bit < 8; ++bit) {
      const bool differ = bool(crc & 0x8000U) != bool(x & 0x80U);
      crc = static_cast<uint16_t>((crc << 1) ^ (differ ? 0x3D65U : 0U));
      x = static_cast<uint8_t>(x << 1);
    }
  }
  return static_cast<uint16_t>(~crc);
}

// raw is the frame B DLL starting at L, without 54 3D radio sync.
inline Integrity probe_format_b_crc(const uint8_t *raw, size_t size) {
  if (!raw || size < 12) return Integrity::TooShort;
  if (size != static_cast<size_t>(raw[0]) + 1) return Integrity::InvalidLength;
  const size_t first = size <= 128 ? size - 2 : 126;
  const uint16_t crc = static_cast<uint16_t>(
      (static_cast<uint16_t>(raw[first]) << 8) | raw[first + 1]);
  if (crc != en13757_crc(raw, first)) return Integrity::CrcMismatch;
  if (size > 128) {
    const size_t count = size - 130;
    const uint16_t second = static_cast<uint16_t>(
        (static_cast<uint16_t>(raw[size - 2]) << 8) | raw[size - 1]);
    if (second != en13757_crc(raw + 128, count))
      return Integrity::CrcMismatch;
  }
  return Integrity::Valid;
}
} // namespace c1
} // namespace wmbus_native_sx126x
} // namespace esphome
