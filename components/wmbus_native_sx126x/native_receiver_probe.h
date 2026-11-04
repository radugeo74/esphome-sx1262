#pragma once
#include <cstdint>
#include <vector>
#include <string>

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/sx126x/sx126x.h"
#include "native_packet_input.h"
#include "t1_fifo_contract.h"

namespace esphome {
namespace wmbus_native_sx126x {

// Passive observer: never registers wmbus meter handlers, never publishes values.
// SX127x can later adapt the same wmbus_radio::native_radio::PacketView contract.
class NativeReceiverProbe : public Component, public sx126x::SX126xListener {
 public:
  void set_radio(sx126x::SX126x *radio) { this->radio_ = radio; }
  void set_fifo_progress_probe(bool value) { this->fifo_progress_probe_ = value; }
#ifdef SX126X_EXPERIMENTAL_PROGRESS_PROBE
  static void on_rx_fifo_progress(uint8_t fifo_count, uint8_t fifo_start);
#endif
  static void on_combined_stream(void *context, const uint8_t *data, size_t count, bool begin, bool end, float rssi_dbm);
  static void on_c1_stream(void *context, const uint8_t *data, size_t count, bool begin, bool end, float rssi_dbm);
  static void on_t1_stream(void *context, const uint8_t *data, size_t count, bool begin, bool end, float rssi_dbm);
  void set_t1_mode(bool value) { this->t1_mode_ = value; }
  void set_combined_mode(bool value) { this->combined_mode_ = value; }
  void set_t1_raw_diagnostics(bool value) { this->t1_raw_diagnostics_ = value; }
  void set_c1_format_b(bool value) { this->c1_format_b_ = value; }
  void add_configured_meter_id(const std::string &value) { this->configured_meter_ids_.push_back(value); }
  void add_meter_driver(const std::string &id, const std::string &driver) { this->meter_drivers_.push_back({id, driver}); }
  void add_xmq_sensor(const std::string &meter_id, const std::string &field,
                      sensor::Sensor *output) {
    this->xmq_sensors_.push_back({meter_id, field, output});
  }
  void add_rssi_sensor(const std::string &meter_id, sensor::Sensor *output) {
    this->rssi_sensors_.push_back({meter_id, output});
  }
  void add_verified_trigger(Trigger<uint32_t, float, std::string, uint32_t> *trigger) {
    this->verified_triggers_.push_back(trigger);
  }
  void setup() override;
  void on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) override;

 protected:
  sx126x::SX126x *radio_{nullptr};
  bool c1_format_b_{true};
  bool fifo_progress_probe_{false};
  bool t1_mode_{false};
  bool combined_mode_{false};
  bool t1_raw_diagnostics_{false};
  struct RxModeStats { uint32_t ok{0}; uint32_t rejected{0}; };
  RxModeStats t1_stats_{};
  RxModeStats c1a_stats_{};
  RxModeStats c1b_stats_{};
  uint32_t combined_stats_last_ms_{0};
  void log_combined_stats_();
  std::vector<std::string> configured_meter_ids_{};
  struct MeterDriver { std::string id; std::string driver; std::string logged_auto_driver; };
  std::vector<MeterDriver> meter_drivers_{};
  t1::FifoContract t1_fifo_{};
  std::vector<uint8_t> c1_encoded_frame_{};
  std::vector<uint8_t> combined_encoded_frame_{};
  struct XmqSensorBinding {
    std::string meter_id;
    std::string field;
    sensor::Sensor *output;
  };
  std::vector<XmqSensorBinding> xmq_sensors_{};
  struct RssiSensorBinding { std::string meter_id; sensor::Sensor *output; };
  std::vector<RssiSensorBinding> rssi_sensors_{};
  std::vector<Trigger<uint32_t, float, std::string, uint32_t> *> verified_triggers_{};
  void fire_verified_(uint32_t meter_id, float rssi, const std::vector<uint8_t> &bytes);
  uint32_t accepted_{0};
  uint32_t rejected_{0};
};

class VerifiedFrameTrigger : public Trigger<uint32_t, float, std::string, uint32_t> {
 public:
  explicit VerifiedFrameTrigger(NativeReceiverProbe *receiver) {
    receiver->add_verified_trigger(this);
  }
};

}  // namespace wmbus_native_sx126x
}  // namespace esphome
