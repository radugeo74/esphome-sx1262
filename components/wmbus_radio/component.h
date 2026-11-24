#pragma once

#include <functional>
#include <array>
#include <cstdint>
#include <cstddef>
#include <vector>


#include "esphome/core/component.h"

// ESPHome only copies selected radio components into the generated build.
// Keep the opposite radio header optional in single-radio configurations.
#if __has_include("esphome/components/sx127x/sx127x.h")
#include "esphome/components/sx127x/sx127x.h"
#define WMBUS_RADIO_HAS_SX127X 1
#endif

#include "packet.h"

namespace esphome {
namespace sx126x { class SX126x; }
namespace sx127x { class SX127x; class SX127xListener; }
namespace wmbus_radio {

class Radio : public Component
#ifdef WMBUS_RADIO_HAS_SX127X
            , public sx127x::SX127xListener
#endif
{
public:
  // The patched SX126x is the ONLY SPI/IRQ owner in the native path.
  // No legacy radio transceiver is created for this configuration.
  void set_native_sx126x(sx126x::SX126x *radio) { this->native_sx126x_ = radio; }
  void set_native_sx127x(sx127x::SX127x *radio) { this->native_sx127x_ = radio; }
#ifdef WMBUS_RADIO_HAS_SX127X
  void on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) override;
#endif
  void set_native_t1_c1_b(bool enabled) { this->native_t1_c1_b_ = enabled; }
  void set_native_phy_diagnostics(bool enabled) { this->native_phy_diagnostics_ = enabled; }
  void set_frame_logging(bool enabled) { this->frame_logging_ = enabled; }
  // 0 = off, 1 = unhandled only, 2 = all verified frames.
  void set_analyzer_links(uint8_t mode) { this->analyzer_links_ = mode; }

  void set_direct_xmq_only(bool enable) { this->direct_xmq_only_ = enable; }
  void setup() override;
  void loop() override;
  void note_xmq_decoded(bool water) { ++this->stats_.xmq_decoded[water ? 1 : 0]; }

  void add_frame_handler(std::function<void(Frame *)> &&callback);

protected:
  void dispatch_frame_(Frame &frame);
  static void sx127x_stream_callback_(void *context, const uint8_t *data,
                                      size_t length, float rssi);
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
  bool frame_logging_{false}; // Per-frame logs are opt-in to avoid logger pressure.
  bool direct_xmq_only_{false}; // YAML API compatibility; XMQ-only dispatch is unconditional.
  uint8_t analyzer_links_{1};
  struct AnalyzerSeen { uint32_t id{0}; uint32_t when{0}; bool used{false}; };
  std::array<AnalyzerSeen, 64> analyzer_recent_{};
  size_t analyzer_next_{0};
  uint32_t c1_a_verified_{0};
  uint32_t c1_b_verified_{0};
  uint32_t c1_crc_rejected_{0};
  uint32_t c1_length_rejected_{0};
  struct PhyStats {
    uint32_t t1_valid{0};
    uint32_t t1_symbol_bad{0};
    uint32_t t1_length_bad{0};
    uint32_t t1_crc_bad{0};
    uint32_t c1_valid[2]{0, 0};
    uint32_t c1_crc_bad{0};
    uint32_t c1_length_bad{0};
    uint32_t xmq_decoded[2]{0, 0}; // heat, water
  };
  PhyStats stats_{};
  PhyStats stats_previous_{};
  uint32_t stats_last_report_ms_{0};
  static constexpr size_t MAX_NATIVE_PHY_SIZE = 384;
  std::array<uint8_t, MAX_NATIVE_PHY_SIZE> native_phy_{};
  size_t native_phy_size_{0};
  bool native_phy_active_{false};

  std::vector<std::function<void(Frame *)>> handlers_;
};
} // namespace wmbus_radio
} // namespace esphome
