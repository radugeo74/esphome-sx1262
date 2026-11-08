#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include <string>

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/sx126x/sx126x.h"
#include "native_packet_input.h"
#include "driver_runtime.h"
#include "t1_fifo_contract.h"
#include "secure_meter_binding.h"

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
  void set_c1_plaintext_publish(bool value) { this->c1_plaintext_publish_ = value; }
  void add_configured_meter_id(const std::string &value) { this->configured_meter_ids_.push_back(value); }
  void add_meter_driver(const std::string &id, const std::string &driver,
                        const std::string &dll_id) {
    this->meter_drivers_.push_back({id, driver, {}, dll_id});
  }
  // The host-tested admission function uses the same driver allowlist and
  // distinct-DLL policy as the YAML schema. Invalid bindings fail closed.
  // Only parsed 16-byte keys are retained; no key is logged.
  void add_secure_meter(const std::string &id,const std::string &driver,
                        const std::string &key_hex,
                        const std::string &dll_id) {
    secure_bindings::add(this->secure_meters_,id,driver,key_hex,dll_id);
  }
  void add_xmq_sensor(const std::string &meter_id, const std::string &field,
                      sensor::Sensor *output) {
    this->xmq_sensors_.push_back({meter_id, field, output});
  }
  void add_xmq_text_sensor(const std::string &meter_id, const std::string &field,
                           text_sensor::TextSensor *output) {
    this->xmq_text_sensors_.push_back({meter_id, field, output});
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
  bool c1_plaintext_publish_{false};
  bool fifo_progress_probe_{false};
  bool t1_mode_{false};
  bool combined_mode_{false};
  bool t1_raw_diagnostics_{false};
  struct RxModeStats { uint32_t ok{0}; uint32_t rejected{0}; };
  RxModeStats t1_stats_{};
  RxModeStats c1a_stats_{};
  RxModeStats c1b_stats_{};
  uint32_t combined_stats_last_ms_{0};
  struct Batch2Seen {
    uint32_t dll_id{0};
    uint32_t target_id{0};
    const char *driver{nullptr};
    uint8_t source{0};
    uint8_t tpl_mode{0xFF};
    uint8_t tpl_security{0};
  };
  // Fixed-capacity log deduplication; no per-telegram heap accumulation.
  std::array<Batch2Seen,32> batch2_seen_{};
  size_t batch2_seen_count_{0};
  size_t batch2_seen_cursor_{0};
  void log_combined_stats_();
  // Diagnostic only: metadata recognition from DLL CRC-verified T1/C1.
  // No dispatch/decoder/key lookup or Home Assistant publication.
  void log_batch2_auto_(const std::vector<uint8_t> &frame,
                        uint32_t dll_meter_id,const char *link);
  void publish_xmq_text_(const std::vector<uint8_t> &frame,
                         const std::string &id, const char *driver,
                         const xmq_portable::DecodedPayload &decoded);
  void publish_c1_plaintext_(const std::vector<uint8_t> &frame, uint32_t meter_id,
                             float rssi_dbm);
  // Called ONLY with CRC-verified, stripped frames from SX1262.
  // true means the explicitly configured secure meter was handled,
  // including failed/blocked decode (no plaintext fallback).
  bool publish_secure_batch2_(const std::vector<uint8_t> &frame,
                              uint32_t meter_id,float rssi_dbm);
  // Explicitly configured unencrypted Minomess/Kamwater/Apator162 ONLY.
  // Returns true for matching binding even on rejected/decrypted-looking
  // input, guaranteeing no fallback to AUTO or keyed decoding.
  bool publish_plain_batch2_(const std::vector<uint8_t> &frame,
                             uint32_t dll_meter_id,float rssi_dbm,
                             bool publication_enabled);
  std::vector<std::string> configured_meter_ids_{};
  struct MeterDriver {
    std::string id; // HA sensor binding / Minomess TPL meter
    std::string driver;
    std::string logged_auto_driver;
    std::string dll_id; // separately configured radio sender
  };
  std::vector<MeterDriver> meter_drivers_{};
  using SecureMeter = secure_bindings::Entry;
  std::vector<SecureMeter> secure_meters_{};
  t1::FifoContract t1_fifo_{};
  std::vector<uint8_t> c1_encoded_frame_{};
  std::vector<uint8_t> combined_encoded_frame_{};
  struct XmqSensorBinding {
    std::string meter_id;
    std::string field;
    sensor::Sensor *output;
  };
  std::vector<XmqSensorBinding> xmq_sensors_{};
  struct XmqTextSensorBinding {
    std::string meter_id;
    std::string field;
    text_sensor::TextSensor *output;
  };
  std::vector<XmqTextSensorBinding> xmq_text_sensors_{};

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
