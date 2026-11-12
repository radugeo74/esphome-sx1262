#pragma once

#include "sx127x_reg.h"
#include "esphome/components/spi/spi.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include <vector>
#include <array>
#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

namespace esphome::sx127x {

static constexpr size_t SX127X_MAX_PACKET_SIZE = 255;  // the payload length register is 8 bits

enum SX127xBw : uint8_t {
  SX127X_BW_2_6,
  SX127X_BW_3_1,
  SX127X_BW_3_9,
  SX127X_BW_5_2,
  SX127X_BW_6_3,
  SX127X_BW_7_8,
  SX127X_BW_10_4,
  SX127X_BW_12_5,
  SX127X_BW_15_6,
  SX127X_BW_20_8,
  SX127X_BW_25_0,
  SX127X_BW_31_3,
  SX127X_BW_41_7,
  SX127X_BW_50_0,
  SX127X_BW_62_5,
  SX127X_BW_83_3,
  SX127X_BW_100_0,
  SX127X_BW_125_0,
  SX127X_BW_166_7,
  SX127X_BW_200_0,
  SX127X_BW_250_0,
  SX127X_BW_500_0,
};

enum class SX127xError { NONE = 0, TIMEOUT, INVALID_PARAMS };
enum class WmbusDecodeResult : uint8_t {
  T1_VERIFIED, C1A_VERIFIED, C1B_VERIFIED,
  BAD_SYMBOL, BAD_CRC, BAD_LENGTH
};

class SX127xListener {
 public:
  virtual void on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) = 0;
};

class SX127x final : public Component,
                     public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                           spi::DATA_RATE_8MHZ> {
 public:
  size_t get_max_packet_size();
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }
  void setup() override;
  void loop() override;
  void dump_config() override;
  void set_auto_cal(bool auto_cal) { this->auto_cal_ = auto_cal; }
  void set_bandwidth(SX127xBw bandwidth) { this->bandwidth_ = bandwidth; }
  void set_bitrate(uint32_t bitrate) { this->bitrate_ = bitrate; }
  void set_bitsync(bool bitsync) { this->bitsync_ = bitsync; }
  void set_coding_rate(uint8_t coding_rate) { this->coding_rate_ = coding_rate; }
  void set_crc_enable(bool crc_enable) { this->crc_enable_ = crc_enable; }
  void set_deviation(uint32_t deviation) { this->deviation_ = deviation; }
  void set_dio0_pin(InternalGPIOPin *dio0_pin) { this->dio0_pin_ = dio0_pin; }
  void set_frequency(uint32_t frequency) { this->frequency_ = frequency; }
  void set_mode_rx();
  void set_mode_tx();
  void set_mode_standby();
  void set_mode_sleep();
  void set_modulation(uint8_t modulation) { this->modulation_ = modulation; }
  void set_pa_pin(uint8_t pin) { this->pa_pin_ = pin; }
  void set_pa_power(uint8_t power) { this->pa_power_ = power; }
  void set_pa_ramp(uint8_t ramp) { this->pa_ramp_ = ramp; }
  void set_packet_mode(bool packet_mode) { this->packet_mode_ = packet_mode; }
  void set_payload_length(uint8_t payload_length) { this->payload_length_ = payload_length; }
  void set_preamble_errors(uint8_t preamble_errors) { this->preamble_errors_ = preamble_errors; }
  void set_preamble_polarity(uint8_t preamble_polarity) { this->preamble_polarity_ = preamble_polarity; }
  void set_preamble_size(uint16_t preamble_size) { this->preamble_size_ = preamble_size; }
  void set_preamble_detect(uint8_t preamble_detect) { this->preamble_detect_ = preamble_detect; }
  void set_rst_pin(InternalGPIOPin *rst_pin) { this->rst_pin_ = rst_pin; }
  void set_rx_floor(float floor) { this->rx_floor_ = floor; }
  void set_rx_start(bool start) { this->rx_start_ = start; }
  void set_shaping(uint8_t shaping) { this->shaping_ = shaping; }
  void set_spreading_factor(uint8_t spreading_factor) { this->spreading_factor_ = spreading_factor; }
  void set_sync_value(const std::vector<uint8_t> &sync_value) { this->sync_value_ = sync_value; }
  void run_image_cal();
  void configure();
  SX127xError transmit_packet(const uint8_t *data, size_t len);
  SX127xError transmit_packet(const std::vector<uint8_t> &packet) {
    return this->transmit_packet(packet.data(), packet.size());
  }
  void register_listener(SX127xListener *listener) { this->listeners_.push_back(listener); }
  // Native wM-Bus T1: drain FIFO continuously, before its 64-byte capacity
  // is reached. The callback runs on the ESPHome main loop, not the ISR.
  using WmbusStreamCallback = void (*)(void *, const uint8_t *, size_t, float);
  void set_wmbus_t1_mode(bool value) { this->wmbus_t1_mode_ = value; }
  void set_wmbus_c1_mode(bool value) { this->wmbus_c1_mode_ = value; }
  void set_wmbus_diagnostics(bool enabled) { this->wmbus_phy_diagnostics_ = enabled; }
  void note_wmbus_decode_result(WmbusDecodeResult event);
  void set_dio1_pin(InternalGPIOPin *pin) { this->dio1_pin_ = pin; }
  void set_wmbus_stream_callback(WmbusStreamCallback callback, void *context) {
    this->wmbus_stream_callback_ = callback;
    this->wmbus_stream_context_ = context;
  }
  Trigger<std::vector<uint8_t>, float, float> *get_packet_trigger() { return &this->packet_trigger_; }

 protected:
  static void IRAM_ATTR gpio_intr(SX127x *arg);
  static void IRAM_ATTR wmbus_fifo_intr_(SX127x *arg);
  static void wmbus_task_entry_(void *ctx);
  void wmbus_task_();
  enum class WmbusReadStatus : uint8_t { COMPLETE, TIMEOUT, OVERRUN };
  WmbusReadStatus wmbus_read_fifo_(uint8_t *buffer, size_t length,
                                   int64_t deadline_us);
  void wmbus_restart_rx_();
  void wmbus_log_statistics_();
  void configure_fsk_ook_();
  void configure_lora_();
  void set_mode_(uint8_t modulation, uint8_t mode);
  void write_fifo_(const uint8_t *data, size_t len);
  void read_fifo_(std::vector<uint8_t> &packet);
  void write_register_(uint8_t reg, uint8_t value);
  void call_listeners_(const std::vector<uint8_t> &packet, float rssi, float snr);
  uint8_t read_register_(uint8_t reg);
  Trigger<std::vector<uint8_t>, float, float> packet_trigger_;
  std::vector<SX127xListener *> listeners_;
  std::vector<uint8_t> packet_;
  std::vector<uint8_t> sync_value_;
  InternalGPIOPin *dio0_pin_{nullptr};
  InternalGPIOPin *rst_pin_{nullptr};
  InternalGPIOPin *dio1_pin_{nullptr};
  static constexpr size_t WMBUS_MAX_RAW_SIZE = 384;
  struct WmbusRxPacket {
    uint16_t size{0};
    int16_t rssi_half_dbm{0};
    std::array<uint8_t, WMBUS_MAX_RAW_SIZE> bytes{};
  };
  WmbusStreamCallback wmbus_stream_callback_{nullptr};
  void *wmbus_stream_context_{nullptr};
  QueueHandle_t wmbus_rx_queue_{nullptr};
  TaskHandle_t wmbus_task_handle_{nullptr};
  bool wmbus_t1_mode_{false};
  bool wmbus_c1_mode_{false};
  bool wmbus_phy_diagnostics_{false};
  uint32_t wmbus_last_log_ms_{0};

  // Boot-only hardware snapshot, safe to report on late ESPHome API reconnect.
  // Never touch the radio SPI bus from dump_config() during active FIFO RX.
  struct WmbusFskRegisters {
    uint8_t rx_bw{0};
    uint8_t afc_bw{0};
    uint8_t rx_config{0};
    uint8_t afc_fei{0};
    uint8_t preamble_detect{0};
    uint8_t sync_config{0};
    uint8_t rssi_thresh{0};
    bool captured{false};
    bool matches_expected{false};
  } wmbus_fsk_registers_{};

  // Atomic since the FIFO task and ESPHome main loop run independently.
  // No meter addresses, payloads or keys are stored in these statistics.
  struct WmbusPhyCounters {
    std::atomic<uint32_t> rx_starts{0};
    std::atomic<uint32_t> t1_candidates{0};
    std::atomic<uint32_t> c1a_candidates{0};
    std::atomic<uint32_t> c1b_candidates{0};
    std::atomic<uint32_t> frames_queued{0};
    std::atomic<uint32_t> t1_verified{0};
    std::atomic<uint32_t> c1a_verified{0};
    std::atomic<uint32_t> c1b_verified{0};
    std::atomic<uint32_t> bad_symbols{0};
    std::atomic<uint32_t> bad_crc{0};
    std::atomic<uint32_t> bad_length{0};
    std::atomic<uint32_t> bad_marker{0};
    std::atomic<uint32_t> bad_prefix{0};
    std::atomic<uint32_t> fifo_overruns{0};
    std::atomic<uint32_t> frame_timeouts{0};
    std::atomic<uint32_t> modem_timeouts{0};
    std::atomic<uint32_t> rx_state_recoveries{0};
    std::atomic<uint32_t> missed_irq_wakeups{0};
    std::atomic<uint32_t> queue_drops{0};
    std::atomic<uint32_t> rx_rearms{0};
  } wmbus_stats_{};

  SX127xBw bandwidth_;
  uint32_t bitrate_;
  uint32_t deviation_;
  uint32_t frequency_;
  uint32_t payload_length_;
  uint16_t preamble_size_;
  uint8_t coding_rate_;
  uint8_t modulation_;
  uint8_t pa_pin_;
  uint8_t pa_power_;
  uint8_t pa_ramp_;
  uint8_t preamble_detect_;
  uint8_t preamble_errors_;
  uint8_t preamble_polarity_;
  uint8_t shaping_;
  uint8_t spreading_factor_;
  float rx_floor_;
  bool auto_cal_{false};
  bool bitsync_{false};
  bool crc_enable_{false};
  bool packet_mode_{false};
  bool rx_start_{false};
};

}  // namespace esphome::sx127x
