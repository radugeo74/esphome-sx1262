#pragma once
// Independent, bounded EN 13757 short application-header reader.
// Does not decrypt, select drivers or parse application data.
#include <cstddef>
#include <cstdint>

namespace wmbus_pure {
enum class HeaderStatus : uint8_t { Ok, TooShort, InvalidLength, InvalidId, InvalidManufacturer };
struct Header {
  uint8_t control;
  uint16_t manufacturer;
  uint32_t meter_id;
  uint8_t version;
  uint8_t device_type;
  uint8_t ci;
  const uint8_t *application;
  size_t application_length;
};
inline HeaderStatus parse_header(const uint8_t *frame, size_t size, Header &out) {
  out = {};
  if (!frame || size < 11) return HeaderStatus::TooShort;
  if (static_cast<size_t>(frame[0]) + 1 != size)
    return HeaderStatus::InvalidLength;
  uint32_t id = 0;
  for (int i = 7; i >= 4; --i) {
    const uint8_t low = frame[i] & 0x0f;
    const uint8_t high = frame[i] >> 4;
    if (low > 9 || high > 9) return HeaderStatus::InvalidId;
    id = id * 100 + high * 10 + low;
  }
  // Manufacturer letters are encoded in three 5-bit A-Z values.\n  const uint16_t manufacturer = static_cast<uint16_t>(frame[2]) |\n                                (static_cast<uint16_t>(frame[3]) << 8);\n  if (((manufacturer >> 10) & 31u) == 0 ||\n      ((manufacturer >> 5) & 31u) == 0 ||\n      (manufacturer & 31u) == 0 ||\n      ((manufacturer >> 10) & 31u) > 26 ||\n      ((manufacturer >> 5) & 31u) > 26 ||\n      (manufacturer & 31u) > 26)\n    return HeaderStatus::InvalidManufacturer;\n  out.control = frame[1];
  out.manufacturer = static_cast<uint16_t>(frame[2]) |
                     (static_cast<uint16_t>(frame[3]) << 8);
  out.meter_id = id;
  out.version = frame[8];
  out.device_type = frame[9];
  out.ci = frame[10];
  out.application = frame + 11;
  out.application_length = size - 11;
  return HeaderStatus::Ok;
}
}  // namespace wmbus_pure
