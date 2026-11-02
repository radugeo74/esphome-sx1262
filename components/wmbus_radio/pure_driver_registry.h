#pragma once
// Driver selection is data-driven: no meter-specific decoder dependencies.
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace wmbus_pure {
struct DriverSignature {
  uint16_t manufacturer;  // EN 13757 manufacturer code, little-endian on wire
  uint8_t version;
  uint8_t device_type;
  const char *driver;
};
enum class DriverSelectionStatus : uint8_t { Found, Unknown, Ambiguous, InvalidArguments };
struct DriverSelection {
  DriverSelectionStatus status;
  const char *driver;
  size_t matches;
};
inline DriverSelection select_driver(const DriverSignature *signatures, size_t count,
                                     uint16_t manufacturer, uint8_t version,
                                     uint8_t device_type) {
  if (!signatures && count) return {DriverSelectionStatus::InvalidArguments, nullptr, 0};
  const char *selected = nullptr;
  size_t matches = 0;
  for (size_t i = 0; i < count; ++i) {
    const auto &s = signatures[i];
    if (s.manufacturer != manufacturer || s.version != version ||
        s.device_type != device_type || !s.driver || !s.driver[0])
      continue;
    if (!selected) {
      selected = s.driver;
      matches = 1;
    } else if (std::strcmp(selected, s.driver) != 0) {
      ++matches;
    }
  }
  if (!selected) return {DriverSelectionStatus::Unknown, nullptr, 0};
  if (matches > 1) return {DriverSelectionStatus::Ambiguous, nullptr, matches};
  return {DriverSelectionStatus::Found, selected, 1};
}
}  // namespace wmbus_pure
