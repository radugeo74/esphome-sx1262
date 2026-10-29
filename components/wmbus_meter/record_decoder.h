#pragma once
#include <cstdint>
#include <cstddef>

namespace xmq_portable {
enum class Unit : uint8_t { VolumeM3, Hca, TemperatureC, RawCounter };
enum class Status : uint8_t { Ok, InvalidKey, UnsupportedVif, InvalidLength };
struct Record { uint16_t raw; double scaled; Unit unit; };

inline int hex_digit(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}
inline Status decode_record(const char *key, size_t key_len,
                            const uint8_t *bytes, size_t length,
                            Record &out) {
  if (!key || !bytes || key_len < 4 || key_len % 2 != 0) return Status::InvalidKey;
  if (length != 2) return Status::InvalidLength;
  for (size_t i = 0; i < key_len; ++i)
    if (hex_digit(key[i]) < 0) return Status::InvalidKey;
  const uint16_t raw = static_cast<uint16_t>(bytes[0]) |
                       (static_cast<uint16_t>(bytes[1]) << 8);
  const unsigned vif = static_cast<unsigned>(hex_digit(key[key_len-2]) * 16 +
                                            hex_digit(key[key_len-1]));
  out.raw = raw;
  if (key_len >= 6 && key[key_len-4] == 'F' && key[key_len-3] == 'D' && vif == 0x3A) {
    out.unit = Unit::RawCounter; out.scaled = raw; return Status::Ok;
  }
  if (vif >= 0x10 && vif <= 0x17) {
    double scale = 0.000001;
    for (unsigned v = 0x10; v < vif; ++v) scale *= 10.0;
    out.unit = Unit::VolumeM3; out.scaled = raw * scale; return Status::Ok;
  }
  if (vif == 0x6E) {
    out.unit = Unit::Hca; out.scaled = raw; return Status::Ok;
  }
  if (vif == 0x65 || vif == 0x5D) {
    out.unit = Unit::TemperatureC; out.scaled = raw / 100.0; return Status::Ok;
  }
  return Status::UnsupportedVif;
}
}  // namespace xmq_portable
