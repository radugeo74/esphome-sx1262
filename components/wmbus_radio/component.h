#pragma once

#include <functional>
#include <array>
#include <cstdint>
#include <cstddef>
#include <vector>


#include "esphome/core/component.h"

#include "esphome/components/wmbus_common/wmbus.h"
#include "esphome/components/sx127x/sx127x.h"
#include "esphome/components/wmbus_common/address.h"

#include "packet.h"

namespace esphome {
namespace sx126x { class SX126x; }
namespace sx127x { class SX127x; class SX127xListener; }
namespace wmbus_radio {

class Radio : public Component, public sx127x::SX127xListener {
public:
  // The patched SX126x is the ONLY SPI/IRQ owner in the native path.
  // No legacy radio transceiver is created for this configuration.
  void set_native_sx126x(sx126x::SX126x *radio) { this->native_sx126x_ = radio; }
  void set_native_sx127x(sx127x::SX127x *radio) { this->native_sx127x_ = radio; }
  void on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) override;
  void set_native_t1_c1_b(bool enabled) { this->native_t1_c1_b_ = enabled; }
  void set_native_phy_diagnostics(bool enabled) { this->native_phy_diagnostics_ = enabled; }

  void setup() override;
  void loop() override;

  void add_frame_handler(std::function<void(Frame *)> &&callback);
  // Meter callbacks are routed only when DLL/TPL addresses match.
  void add_meter_frame_handler(std::vector<AddressExpression> addresses,
                               std::function<void(Frame *)> &&callback);

protected:
  void dispatch_frame_(Frame &frame);
  static void native_stream_callback_(void *context, const uint8_t *data,
                                      size_t length, bool start, bool end,
                                      float rssi);
  void native_stream_(const uint8_t *data, size_t length,
                      bool start, bool end, float rssi);
  void decode_native_stream_(float rssi);

  sx126x::SX126x *native_sx126x_{nullptr};
  sx127x::SX127x *native_sx127x_{nullptr};
  bool native_t1_c1_b_{false};
  bool native_phy_diagnostics_{false};
  uint32_t c1_a_verified_{0};
  uint32_t c1_b_verified_{0};
  uint32_t c1_crc_rejected_{0};
  uint32_t c1_length_rejected_{0};
  static constexpr size_t MAX_NATIVE_PHY_SIZE = 384;
  std::array<uint8_t, MAX_NATIVE_PHY_SIZE> native_phy_{};
  size_t native_phy_size_{0};
  bool native_phy_active_{false};

  struct MeterFrameHandler {
    std::vector<AddressExpression> addresses;
    std::function<void(Frame *)> callback;
  };
  std::vector<MeterFrameHandler> meter_handlers_;
  std::vector<std::function<void(Frame *)>> handlers_;
};
} // namespace wmbus_radio
} // namespace esphome
