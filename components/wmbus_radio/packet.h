#pragma once
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <optional>
#include <string>
#include <vector>

#include "esphome/components/wmbus_common/wmbus.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace wmbus_radio {
struct Frame {
public:
  // Verified, CRC-stripped frame delivered by the patched native SX126x bridge.
  Frame(std::vector<uint8_t> &&data, LinkMode mode, int8_t rssi,
        const char *format);

  std::vector<uint8_t> &data();
  LinkMode link_mode();
  int8_t rssi();
  std::string format();

  std::vector<uint8_t> as_raw();
  std::string as_hex();
  std::string as_rtlwmbus();

  void mark_as_handled();
  uint8_t handlers_count();

protected:
  std::vector<uint8_t> data_;
  LinkMode link_mode_;
  int8_t rssi_;
  std::string format_;
  uint8_t handlers_count_ = 0;
};

} // namespace wmbus_radio
} // namespace esphome
