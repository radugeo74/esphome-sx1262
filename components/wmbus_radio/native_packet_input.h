#pragma once
// Transport-independent input boundary for native ESPHome radio packet events.
// This header deliberately has no SX126x/SX127x dependency. The official radio
// component owns hardware state; upper layers see only complete raw RX bytes.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace esphome {
namespace wmbus_radio {
namespace native_radio {

static constexpr size_t MAX_RAW_PACKET_SIZE = 255;

enum class InputStatus : uint8_t {
  Ok,
  Empty,
  Oversize,
  InvalidRssi,
};

struct PacketView {
  const uint8_t *bytes{nullptr};
  size_t length{0};
  float rssi_dbm{0};
  float snr_db{0};
};

inline InputStatus validate(const PacketView &p) {
  if (p.length == 0 || p.bytes == nullptr)
    return InputStatus::Empty;
  if (p.length > MAX_RAW_PACKET_SIZE)
    return InputStatus::Oversize;
  if (!std::isfinite(p.rssi_dbm))
    return InputStatus::InvalidRssi;
  return InputStatus::Ok;
}

// Create owned input bytes so callback-owned memory is never referenced later.
// No attempt to infer T1/C1 or to accept a wM-Bus frame: these remain tasks for
// the existing frame converter and CRC/security validator.
inline InputStatus copy_validated(const PacketView &incoming,
                                  std::vector<uint8_t> &owned) {
  owned.clear();
  const auto status = validate(incoming);
  if (status != InputStatus::Ok)
    return status;
  owned.assign(incoming.bytes, incoming.bytes + incoming.length);
  return InputStatus::Ok;
}

}  // namespace native_radio
}  // namespace wmbus_radio
}  // namespace esphome
