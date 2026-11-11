#pragma once

#include <functional>
#include <array>
#include <cstdint>
#include <cstddef>
#include <vector>


#include "esphome/core/component.h"

#include "esphome/components/wmbus_common/wmbus.h"

#include "packet.h"

namespace esphome {
namespace sx126x { class SX126x; }
namespace wmbus_radio {

class Radio : public Component {
public:
  // The patched SX126x is the ONLY SPI/IRQ owner in the native path.
  // No legacy radio transceiver is created for this configuration.
  void set_native_sx126x(sx126x::SX126x *radio) { this->native_sx126x_ = radio; }
  void set_native_t1_c1_b(bool enabled) { this->native_t1_c1_b_ = enabled; }

  void setup() override;
  void loop() override;

  void add_frame_handler(std::function<void(Frame *)> &&callback);

protected:
  void dispatch_frame_(Frame &frame);
#ifdef USE_WMBUS_NATIVE_SX126X
  static void native_stream_callback_(void *context, const uint8_t *data,
                                      size_t length, bool start, bool end,
                                      float rssi);
  void native_stream_(const uint8_t *data, size_t length,
                      bool start, bool end, float rssi);
  void decode_native_stream_(float rssi);
#endif

  sx126x::SX126x *native_sx126x_{nullptr};
  bool native_t1_c1_b_{false};
#ifdef USE_WMBUS_NATIVE_SX126X
  static constexpr size_t MAX_NATIVE_PHY_SIZE = 384;
  std::array<uint8_t, MAX_NATIVE_PHY_SIZE> native_phy_{};
  size_t native_phy_size_{0};
  bool native_phy_active_{false};
#endif

  std::vector<std::function<void(Frame *)>> handlers_;
};
} // namespace wmbus_radio
} // namespace esphome
