#include "sx127x.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esp_timer.h"

namespace esphome::sx127x {

static const char *const TAG = "sx127x";
static const uint32_t FXOSC = 32000000u;
static const uint16_t RAMP[16] = {3400, 2000, 1000, 500, 250, 125, 100, 62, 50, 40, 31, 25, 20, 15, 12, 10};
static const uint32_t BW_HZ[22] = {2604,  3125,  3906,  5208,  6250,  7812,   10416,  12500,  15625,  20833,  25000,
                                   31250, 41666, 50000, 62500, 83333, 100000, 125000, 166666, 200000, 250000, 500000};
static const uint8_t BW_LORA[22] = {BW_7_8,   BW_7_8,   BW_7_8,   BW_7_8,   BW_7_8,   BW_7_8,  BW_10_4, BW_15_6,
                                    BW_15_6,  BW_20_8,  BW_31_3,  BW_31_3,  BW_41_7,  BW_62_5, BW_62_5, BW_125_0,
                                    BW_125_0, BW_125_0, BW_250_0, BW_250_0, BW_250_0, BW_500_0};
static const uint8_t BW_FSK_OOK[22] = {RX_BW_2_6,   RX_BW_3_1,   RX_BW_3_9,   RX_BW_5_2,  RX_BW_6_3,   RX_BW_7_8,
                                       RX_BW_10_4,  RX_BW_12_5,  RX_BW_15_6,  RX_BW_20_8, RX_BW_25_0,  RX_BW_31_3,
                                       RX_BW_41_7,  RX_BW_50_0,  RX_BW_62_5,  RX_BW_83_3, RX_BW_100_0, RX_BW_125_0,
                                       RX_BW_166_7, RX_BW_200_0, RX_BW_250_0, RX_BW_250_0};
static const int32_t RSSI_OFFSET_HF = 157;
static const int32_t RSSI_OFFSET_LF = 164;

uint8_t SX127x::read_register_(uint8_t reg) {
  this->enable();
  this->write_byte(reg & 0x7F);
  uint8_t value = this->read_byte();
  this->disable();
  return value;
}

void SX127x::write_register_(uint8_t reg, uint8_t value) {
  this->enable();
  this->write_byte(reg | 0x80);
  this->write_byte(value);
  this->disable();
}

void SX127x::read_fifo_(std::vector<uint8_t> &packet) {
  this->enable();
  this->write_byte(REG_FIFO & 0x7F);
  for (auto &byte : packet) {
    byte = this->transfer_byte(0x00);
  }
  this->disable();
}

void SX127x::write_fifo_(const uint8_t *data, size_t len) {
  this->enable();
  this->write_byte(REG_FIFO | 0x80);
  for (size_t i = 0; i < len; i++) {
    this->transfer_byte(data[i]);
  }
  this->disable();
}

void IRAM_ATTR SX127x::gpio_intr(SX127x *arg) { arg->enable_loop_soon_any_context(); }

void SX127x::setup() {
  // setup reset
  this->rst_pin_->setup();
  if (this->wmbus_t1_mode_) {
    if (this->dio1_pin_ == nullptr) {
      ESP_LOGE(TAG, "wM-Bus T1 FIFO streaming requires DIO1");
      this->mark_failed();
      return;
    }
    this->dio1_pin_->setup();
  }

  // setup dio0
  if (this->dio0_pin_) {
    this->dio0_pin_->setup();
    this->dio0_pin_->attach_interrupt(&SX127x::gpio_intr, this, gpio::INTERRUPT_RISING_EDGE);
  }

  // start spi
  this->spi_setup();

  // configure rf
  this->configure();

  if (this->wmbus_t1_mode_ && !this->is_failed()) {
    this->wmbus_rx_queue_ = xQueueCreate(3, sizeof(WmbusRxPacket));
    if (this->wmbus_rx_queue_ == nullptr ||
        xTaskCreate(&SX127x::wmbus_task_entry_, "sx127x_wmbus", 6144, this, 19,
                    &this->wmbus_task_handle_) != pdPASS) {
      ESP_LOGE(TAG, "Cannot allocate SX127x FIFO receiver task/queue");
      this->mark_failed();
      return;
    }
    // DIO1=FifoEmpty is active-low; interrupt when the first byte arrives.
    this->dio1_pin_->attach_interrupt(&SX127x::wmbus_fifo_intr_, this,
                                      gpio::INTERRUPT_FALLING_EDGE);
    if (!this->dio1_pin_->digital_read())
      xTaskNotifyGive(this->wmbus_task_handle_);
    ESP_LOGW(TAG, "SX127x wM-Bus T1 FIFO stream enabled (experimental RF)");
  }
}

void SX127x::configure() {
  // toggle chip reset
  this->rst_pin_->digital_write(false);
  delayMicroseconds(1000);
  this->rst_pin_->digital_write(true);
  delayMicroseconds(10000);

  // check silicon version to make sure hw is ok
  if (this->read_register_(REG_VERSION) != 0x12) {
    this->mark_failed();
    return;
  }

  // enter sleep mode
  this->set_mode_(MOD_FSK, MODE_SLEEP);

  // set freq
  uint64_t frf = ((uint64_t) this->frequency_ << 19) / FXOSC;
  this->write_register_(REG_FRF_MSB, (uint8_t) ((frf >> 16) & 0xFF));
  this->write_register_(REG_FRF_MID, (uint8_t) ((frf >> 8) & 0xFF));
  this->write_register_(REG_FRF_LSB, (uint8_t) ((frf >> 0) & 0xFF));

  // enter standby mode
  this->set_mode_(MOD_FSK, MODE_STDBY);

  // run image cal
  this->run_image_cal();

  // go back to sleep
  this->set_mode_sleep();

  // config pa
  if (this->pa_pin_ == PA_PIN_BOOST) {
    this->pa_power_ = std::max(this->pa_power_, (uint8_t) 2);
    this->pa_power_ = std::min(this->pa_power_, (uint8_t) 17);
    this->write_register_(REG_PA_CONFIG, (this->pa_power_ - 2) | this->pa_pin_ | PA_MAX_POWER);
  } else {
    this->pa_power_ = std::min(this->pa_power_, (uint8_t) 14);
    this->write_register_(REG_PA_CONFIG, (this->pa_power_ - 0) | this->pa_pin_ | PA_MAX_POWER);
  }
  if (this->modulation_ != MOD_LORA) {
    this->write_register_(REG_PA_RAMP, this->pa_ramp_ | this->shaping_);
  } else {
    this->write_register_(REG_PA_RAMP, this->pa_ramp_);
  }

  // configure modem
  if (this->modulation_ != MOD_LORA) {
    this->configure_fsk_ook_();
  } else {
    this->configure_lora_();
  }

  // switch to rx or sleep
  if (this->rx_start_) {
    this->set_mode_rx();
  } else {
    this->set_mode_sleep();
  }
}

void SX127x::configure_fsk_ook_() {
  // set the channel bw
  this->write_register_(REG_RX_BW, BW_FSK_OOK[this->bandwidth_]);

  // set fdev
  uint32_t fdev = std::min((this->deviation_ * 4096) / 250000, (uint32_t) 0x3FFF);
  this->write_register_(REG_FDEV_MSB, (uint8_t) ((fdev >> 8) & 0xFF));
  this->write_register_(REG_FDEV_LSB, (uint8_t) ((fdev >> 0) & 0xFF));

  // set bitrate
  uint64_t bitrate = (FXOSC + this->bitrate_ / 2) / this->bitrate_;  // round up
  this->write_register_(REG_BITRATE_MSB, (uint8_t) ((bitrate >> 8) & 0xFF));
  this->write_register_(REG_BITRATE_LSB, (uint8_t) ((bitrate >> 0) & 0xFF));

  // configure rx and afc
  uint8_t trigger = (this->preamble_detect_ > 0) ? TRIGGER_PREAMBLE : TRIGGER_RSSI;
  this->write_register_(REG_AFC_FEI, AFC_AUTO_CLEAR_ON);
  if (this->modulation_ == MOD_FSK) {
    this->write_register_(REG_RX_CONFIG, AFC_AUTO_ON | AGC_AUTO_ON | trigger);
  } else {
    this->write_register_(REG_RX_CONFIG, AGC_AUTO_ON | trigger);
  }

  // Configure raw T1 FIFO streaming. PayloadLength=0 plus fixed
  // packet format lets software drain long packets through DIO1 before
  // the 64-byte hardware FIFO fills. The radio does not generate CRC
  // acceptance: native wM-Bus PHY validation remains mandatory.
  if (this->wmbus_t1_mode_) {
    this->write_register_(REG_FIFO_THRESH, TX_START_FIFO_EMPTY);
    this->write_register_(REG_PACKET_CONFIG_1, FIXED_LENGTH | CRC_OFF);
    this->write_register_(REG_PAYLOAD_LENGTH_LSB, 0);
    this->write_register_(REG_PACKET_CONFIG_2, PACKET_MODE);
  } else if (this->packet_mode_) {
    uint8_t crc_mode = (this->crc_enable_) ? CRC_ON : CRC_OFF;
    this->write_register_(REG_FIFO_THRESH, TX_START_FIFO_EMPTY);
    if (this->payload_length_ > 0) {
      this->write_register_(REG_PAYLOAD_LENGTH_LSB, this->payload_length_);
      this->write_register_(REG_PACKET_CONFIG_1, crc_mode | FIXED_LENGTH);
    } else {
      this->write_register_(REG_PAYLOAD_LENGTH_LSB, this->get_max_packet_size() - 1);
      this->write_register_(REG_PACKET_CONFIG_1, crc_mode | VARIABLE_LENGTH);
    }
    this->write_register_(REG_PACKET_CONFIG_2, PACKET_MODE);
  } else {
    this->write_register_(REG_PACKET_CONFIG_2, CONTINUOUS_MODE);
  }
  this->write_register_(REG_DIO_MAPPING1, this->wmbus_t1_mode_ ? 0x10 : DIO0_MAPPING_00);

  // config bit synchronizer
  uint8_t polarity = (this->preamble_polarity_ == 0xAA) ? PREAMBLE_AA : PREAMBLE_55;
  if (!this->sync_value_.empty()) {
    uint8_t size = this->sync_value_.size() - 1;
    this->write_register_(REG_SYNC_CONFIG, AUTO_RESTART_PLL_LOCK | polarity | SYNC_ON | size);
    for (uint32_t i = 0; i < this->sync_value_.size(); i++) {
      this->write_register_(REG_SYNC_VALUE1 + i, this->sync_value_[i]);
    }
  } else {
    this->write_register_(REG_SYNC_CONFIG, AUTO_RESTART_PLL_LOCK | polarity);
  }

  // config preamble detector
  if (this->preamble_detect_ > 0) {
    uint8_t size = (this->preamble_detect_ - 1) << PREAMBLE_DETECTOR_SIZE_SHIFT;
    uint8_t tol = this->preamble_errors_ << PREAMBLE_DETECTOR_TOL_SHIFT;
    this->write_register_(REG_PREAMBLE_DETECT, PREAMBLE_DETECTOR_ON | size | tol);
  } else {
    this->write_register_(REG_PREAMBLE_DETECT, PREAMBLE_DETECTOR_OFF);
  }
  this->write_register_(REG_PREAMBLE_SIZE_MSB, this->preamble_size_ >> 8);
  this->write_register_(REG_PREAMBLE_SIZE_LSB, this->preamble_size_ & 0xFF);

  // config sync generation and setup ook threshold
  uint8_t bitsync = this->bitsync_ ? BIT_SYNC_ON : BIT_SYNC_OFF;
  this->write_register_(REG_OOK_PEAK, bitsync | OOK_THRESH_STEP_0_5 | OOK_THRESH_PEAK);
  this->write_register_(REG_OOK_AVG, OOK_AVG_RESERVED | OOK_THRESH_DEC_1_8);

  // set rx floor
  this->write_register_(REG_OOK_FIX, 256 + int(this->rx_floor_ * 2.0f));
  this->write_register_(REG_RSSI_THRESH, std::abs(int(this->rx_floor_ * 2.0f)));
}

void SX127x::configure_lora_() {
  // config modem
  uint8_t header_mode = this->payload_length_ > 0 ? IMPLICIT_HEADER : EXPLICIT_HEADER;
  uint8_t crc_mode = (this->crc_enable_) ? RX_PAYLOAD_CRC_ON : RX_PAYLOAD_CRC_OFF;
  uint8_t spreading_factor = this->spreading_factor_ << SPREADING_FACTOR_SHIFT;
  this->write_register_(REG_MODEM_CONFIG1, BW_LORA[this->bandwidth_] | this->coding_rate_ | header_mode);
  this->write_register_(REG_MODEM_CONFIG2, spreading_factor | crc_mode);

  // config fifo and payload length
  this->write_register_(REG_FIFO_TX_BASE_ADDR, 0x00);
  this->write_register_(REG_FIFO_RX_BASE_ADDR, 0x00);
  this->write_register_(REG_PAYLOAD_LENGTH, std::max(this->payload_length_, (uint32_t) 1));

  // config preamble
  if (this->preamble_size_ >= 6) {
    this->write_register_(REG_PREAMBLE_LEN_MSB, this->preamble_size_ >> 8);
    this->write_register_(REG_PREAMBLE_LEN_LSB, this->preamble_size_ & 0xFF);
  }

  // optimize detection
  float duration = 1000.0f * (1UL << this->spreading_factor_) / BW_HZ[this->bandwidth_];
  if (duration > 16) {
    this->write_register_(REG_MODEM_CONFIG3, MODEM_AGC_AUTO_ON | LOW_DATA_RATE_OPTIMIZE_ON);
  } else {
    this->write_register_(REG_MODEM_CONFIG3, MODEM_AGC_AUTO_ON);
  }
  if (this->spreading_factor_ == 6) {
    this->write_register_(REG_DETECT_OPTIMIZE, 0xC5);
    this->write_register_(REG_DETECT_THRESHOLD, 0x0C);
  } else {
    this->write_register_(REG_DETECT_OPTIMIZE, 0xC3);
    this->write_register_(REG_DETECT_THRESHOLD, 0x0A);
  }

  // config sync word
  if (!this->sync_value_.empty()) {
    this->write_register_(REG_SYNC_WORD, this->sync_value_[0]);
  }
}

size_t SX127x::get_max_packet_size() {
  if (this->payload_length_ > 0) {
    return this->payload_length_;
  }
  if (this->modulation_ == MOD_LORA) {
    return SX127X_MAX_PACKET_SIZE;
  }
  return 64;
}

SX127xError SX127x::transmit_packet(const uint8_t *data, size_t len) {
  if (this->payload_length_ > 0 && this->payload_length_ != len) {
    ESP_LOGE(TAG, "Packet size does not match config");
    return SX127xError::INVALID_PARAMS;
  }
  if (len == 0 || len > this->get_max_packet_size()) {
    ESP_LOGE(TAG, "Packet size out of range");
    return SX127xError::INVALID_PARAMS;
  }

  if (this->dio0_pin_ == nullptr) {
    ESP_LOGE(TAG, "DIO0 pin not configured, cannot wait for transmit completion");
    return SX127xError::INVALID_PARAMS;
  }

  SX127xError ret = SX127xError::NONE;
  if (this->modulation_ == MOD_LORA) {
    this->set_mode_standby();
    if (this->payload_length_ == 0) {
      this->write_register_(REG_PAYLOAD_LENGTH, len);
    }
    this->write_register_(REG_IRQ_FLAGS, 0xFF);
    this->write_register_(REG_FIFO_ADDR_PTR, 0);
    this->write_fifo_(data, len);
    this->set_mode_tx();
  } else {
    this->set_mode_standby();
    if (this->payload_length_ == 0) {
      this->write_register_(REG_FIFO, len);
    }
    this->write_fifo_(data, len);
    this->set_mode_tx();
  }

  // wait until transmit completes, typically the delay will be less than 100 ms
  uint32_t start = millis();
  while (!this->dio0_pin_->digital_read()) {
    if (millis() - start > 4000) {
      ESP_LOGE(TAG, "Transmit packet failure");
      ret = SX127xError::TIMEOUT;
      break;
    }
  }
  if (this->rx_start_) {
    this->set_mode_rx();
  } else {
    this->set_mode_sleep();
  }
  return ret;
}

void SX127x::call_listeners_(const std::vector<uint8_t> &packet, float rssi, float snr) {
  for (auto &listener : this->listeners_) {
    listener->on_packet(packet, rssi, snr);
  }
  this->packet_trigger_.trigger(packet, rssi, snr);
}

void SX127x::loop() {
  if (this->wmbus_t1_mode_) {
    if (this->wmbus_rx_queue_ == nullptr)
      return;
    WmbusRxPacket frame;
    // Keep decoding in the ESPHome main loop, never in the high-priority
    // FIFO reader or GPIO ISR.
    while (xQueueReceive(this->wmbus_rx_queue_, &frame, 0) == pdTRUE) {
      if (this->wmbus_stream_callback_ != nullptr)
        this->wmbus_stream_callback_(this->wmbus_stream_context_,
                                     frame.bytes.data(), frame.size,
                                     static_cast<float>(frame.rssi_half_dbm) / 2.0f);
    }
    return;
  }
  this->disable_loop();
  if (this->dio0_pin_ == nullptr || !this->dio0_pin_->digital_read()) {
    return;
  }

  if (this->modulation_ == MOD_LORA) {
    uint8_t status = this->read_register_(REG_IRQ_FLAGS);
    this->write_register_(REG_IRQ_FLAGS, 0xFF);
    if ((status & PAYLOAD_CRC_ERROR) == 0) {
      uint8_t bytes = this->read_register_(REG_NB_RX_BYTES);
      uint8_t addr = this->read_register_(REG_FIFO_RX_CURR_ADDR);
      uint8_t rssi = this->read_register_(REG_PKT_RSSI_VALUE);
      int8_t snr = (int8_t) this->read_register_(REG_PKT_SNR_VALUE);
      this->packet_.resize(bytes);
      this->write_register_(REG_FIFO_ADDR_PTR, addr);
      this->read_fifo_(this->packet_);
      if (this->frequency_ > 700000000) {
        this->call_listeners_(this->packet_, (float) rssi - RSSI_OFFSET_HF, (float) snr / 4);
      } else {
        this->call_listeners_(this->packet_, (float) rssi - RSSI_OFFSET_LF, (float) snr / 4);
      }
    }
  } else if (this->packet_mode_) {
    uint8_t payload_length = this->payload_length_;
    if (payload_length == 0) {
      payload_length = this->read_register_(REG_FIFO);
    }
    this->packet_.resize(payload_length);
    this->read_fifo_(this->packet_);
    this->call_listeners_(this->packet_, 0.0f, 0.0f);
  }
}

void SX127x::run_image_cal() {
  if (this->modulation_ == MOD_LORA) {
    this->set_mode_(MOD_FSK, MODE_SLEEP);
    this->set_mode_(MOD_FSK, MODE_STDBY);
  }
  if (this->auto_cal_) {
    this->write_register_(REG_IMAGE_CAL, IMAGE_CAL_START | AUTO_IMAGE_CAL_ON | TEMP_THRESHOLD_10C);
  } else {
    this->write_register_(REG_IMAGE_CAL, IMAGE_CAL_START);
  }
  uint32_t start = millis();
  while (this->read_register_(REG_IMAGE_CAL) & IMAGE_CAL_RUNNING) {
    if (millis() - start > 20) {
      ESP_LOGE(TAG, "Image cal failure");
      this->mark_failed();
      break;
    }
  }
  if (this->modulation_ == MOD_LORA) {
    this->set_mode_(this->modulation_, MODE_SLEEP);
    this->set_mode_(this->modulation_, MODE_STDBY);
  }
}

void SX127x::set_mode_(uint8_t modulation, uint8_t mode) {
  uint32_t start = millis();
  this->write_register_(REG_OP_MODE, modulation | mode);
  while (true) {
    uint8_t curr = this->read_register_(REG_OP_MODE) & MODE_MASK;
    if ((curr == mode) || (mode == MODE_RX && curr == MODE_RX_FS)) {
      if (mode == MODE_SLEEP) {
        this->write_register_(REG_OP_MODE, modulation | mode);
      }
      break;
    }
    if (millis() - start > 20) {
      ESP_LOGE(TAG, "Set mode failure");
      this->mark_failed();
      return;
    }
  }
}

void SX127x::set_mode_rx() {
  this->set_mode_(this->modulation_, MODE_RX);
  if (this->modulation_ == MOD_LORA) {
    this->write_register_(REG_IRQ_FLAGS_MASK, 0x00);
    this->write_register_(REG_DIO_MAPPING1, DIO0_MAPPING_00);
  }
}

void SX127x::set_mode_tx() {
  this->set_mode_(this->modulation_, MODE_TX);
  if (this->modulation_ == MOD_LORA) {
    this->write_register_(REG_IRQ_FLAGS_MASK, 0x00);
    this->write_register_(REG_DIO_MAPPING1, DIO0_MAPPING_01);
  }
}

void SX127x::set_mode_standby() { this->set_mode_(this->modulation_, MODE_STDBY); }

void SX127x::set_mode_sleep() { this->set_mode_(this->modulation_, MODE_SLEEP); }


void IRAM_ATTR SX127x::wmbus_fifo_intr_(SX127x *arg) {
  if (arg->wmbus_task_handle_ == nullptr)
    return;
  BaseType_t woken = pdFALSE;
  vTaskNotifyGiveFromISR(arg->wmbus_task_handle_, &woken);
  if (woken == pdTRUE)
    portYIELD_FROM_ISR();
}

void SX127x::wmbus_task_entry_(void *ctx) {
  static_cast<SX127x *>(ctx)->wmbus_task_();
}

bool SX127x::wmbus_read_byte_(uint8_t *out) {
  // 100 kbit/s => roughly 80 us per byte. Never read the FIFO while
  // FifoEmpty is asserted; the chip would return stale/undefined bytes.
  const int64_t started = esp_timer_get_time();
  while (this->dio1_pin_->digital_read()) {
    if (esp_timer_get_time() - started >= 1500)
      return false;
    taskYIELD();
  }
  *out = this->read_register_(REG_FIFO);
  return true;
}

int SX127x::wmbus_decode_nibble_(uint8_t code) {
  // EN13757-4 T-mode 3-of-6 symbol map.
  switch (code) {
    case 0b010110: return 0x0;
    case 0b001101: return 0x1;
    case 0b001110: return 0x2;
    case 0b001011: return 0x3;
    case 0b011100: return 0x4;
    case 0b011001: return 0x5;
    case 0b011010: return 0x6;
    case 0b010011: return 0x7;
    case 0b101100: return 0x8;
    case 0b100101: return 0x9;
    case 0b100110: return 0xA;
    case 0b100011: return 0xB;
    case 0b110100: return 0xC;
    case 0b110001: return 0xD;
    case 0b110010: return 0xE;
    case 0b101001: return 0xF;
    default: return -1;
  }
}

void SX127x::wmbus_restart_rx_() {
  // Re-arm hardware synchronization between distinct telegrams.
  this->set_mode_standby();
  this->write_register_(0x3F, 0x10);  // FSK IrqFlags2: FIFO overrun clear
  this->set_mode_rx();
}

void SX127x::wmbus_task_() {
  while (true) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (this->is_failed())
      continue;

    WmbusRxPacket frame;
    bool complete = true;
    // T1 first 3 encoded bytes provide L and C fields (2 decoded bytes).
    for (size_t i = 0; i < 3; i++) {
      if (!this->wmbus_read_byte_(&frame.bytes[i])) {
        complete = false;
        break;
      }
    }
    if (complete) {
      const uint8_t *b = frame.bytes.data();
      const int l_hi = wmbus_decode_nibble_(b[0] >> 2);
      const int l_lo = wmbus_decode_nibble_(((b[0] & 0x03) << 4) | (b[1] >> 4));
      const int c_hi = wmbus_decode_nibble_(((b[1] & 0x0F) << 2) | (b[2] >> 6));
      const int c_lo = wmbus_decode_nibble_(b[2] & 0x3F);
      if (l_hi < 0 || l_lo < 0 || c_hi < 0 || c_lo < 0) {
        complete = false;
      } else {
        const size_t l = (l_hi << 4) | l_lo;
        // Accept conventional wM-Bus send requests only; CRC will still
        // be checked end-to-end in wmbus_radio, before dispatch.
        const uint8_t control = (c_hi << 4) | c_lo;
        if (l < 10 || control != 0x44) {
          complete = false;
        } else {
          const size_t blocks = 1 + (l - 9 + 15) / 16;
          const size_t decoded = l + 1 + 2 * blocks;
          const size_t raw = (3 * decoded + 1) / 2;
          if (raw < 3 || raw > WMBUS_MAX_RAW_SIZE) {
            complete = false;
          } else {
            frame.size = static_cast<uint16_t>(raw);
            // Read signal RSSI while the telegram is still in progress.
            frame.rssi_half_dbm = -static_cast<int16_t>(this->read_register_(0x11));
            for (size_t i = 3; i < raw; i++) {
              if (!this->wmbus_read_byte_(&frame.bytes[i])) {
                complete = false;
                break;
              }
            }
          }
        }
      }
    }
    if (complete && this->wmbus_rx_queue_ != nullptr) {
      if (xQueueSend(this->wmbus_rx_queue_, &frame, 0) != pdTRUE)
        ESP_LOGW(TAG, "SX127x wM-Bus queue full; frame discarded");
    }
    this->wmbus_restart_rx_();
  }
}

void SX127x::dump_config() {
  ESP_LOGCONFIG(TAG, "SX127x:");
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  RST Pin: ", this->rst_pin_);
  LOG_PIN("  DIO0 Pin: ", this->dio0_pin_);
  const char *pa_pin = "RFO";
  if (this->pa_pin_ == PA_PIN_BOOST) {
    pa_pin = "BOOST";
  }
  ESP_LOGCONFIG(TAG,
                "  Auto Cal: %s\n"
                "  Frequency: %" PRIu32 " Hz\n"
                "  Bandwidth: %" PRIu32 " Hz\n"
                "  PA Pin: %s\n"
                "  PA Power: %" PRIu8 " dBm\n"
                "  PA Ramp: %" PRIu16 " us",
                TRUEFALSE(this->auto_cal_), this->frequency_, BW_HZ[this->bandwidth_], pa_pin, this->pa_power_,
                RAMP[this->pa_ramp_]);
  if (this->modulation_ == MOD_FSK) {
    ESP_LOGCONFIG(TAG, "  Deviation: %" PRIu32 " Hz", this->deviation_);
  }
  if (this->modulation_ == MOD_LORA) {
    const char *cr = "4/8";
    if (this->coding_rate_ == CODING_RATE_4_5) {
      cr = "4/5";
    } else if (this->coding_rate_ == CODING_RATE_4_6) {
      cr = "4/6";
    } else if (this->coding_rate_ == CODING_RATE_4_7) {
      cr = "4/7";
    }
    ESP_LOGCONFIG(TAG,
                  "  Modulation: LORA\n"
                  "  Preamble Size: %" PRIu16 "\n"
                  "  Spreading Factor: %" PRIu8 "\n"
                  "  Coding Rate: %s\n"
                  "  CRC Enable: %s",
                  this->preamble_size_, this->spreading_factor_, cr, TRUEFALSE(this->crc_enable_));
    if (this->payload_length_ > 0) {
      ESP_LOGCONFIG(TAG, "  Payload Length: %" PRIu32, this->payload_length_);
    }
    if (!this->sync_value_.empty()) {
      ESP_LOGCONFIG(TAG, "  Sync Value: 0x%02x", this->sync_value_[0]);
    }
  } else {
    const char *shaping = "NONE";
    if (this->modulation_ == MOD_FSK) {
      if (this->shaping_ == GAUSSIAN_BT_0_3) {
        shaping = "GAUSSIAN_BT_0_3";
      } else if (this->shaping_ == GAUSSIAN_BT_0_5) {
        shaping = "GAUSSIAN_BT_0_5";
      } else if (this->shaping_ == GAUSSIAN_BT_1_0) {
        shaping = "GAUSSIAN_BT_1_0";
      }
    } else {
      if (this->shaping_ == CUTOFF_BR_X_2) {
        shaping = "CUTOFF_BR_X_2";
      } else if (this->shaping_ == CUTOFF_BR_X_1) {
        shaping = "CUTOFF_BR_X_1";
      }
    }
    ESP_LOGCONFIG(TAG,
                  "  Shaping: %s\n"
                  "  Modulation: %s\n"
                  "  Bitrate: %" PRIu32 "b/s\n"
                  "  Bitsync: %s\n"
                  "  Rx Start: %s\n"
                  "  Rx Floor: %.1f dBm\n"
                  "  Packet Mode: %s",
                  shaping, this->modulation_ == MOD_FSK ? LOG_STR_LITERAL("FSK") : LOG_STR_LITERAL("OOK"),
                  this->bitrate_, TRUEFALSE(this->bitsync_), TRUEFALSE(this->rx_start_), this->rx_floor_,
                  TRUEFALSE(this->packet_mode_));
    if (this->packet_mode_) {
      ESP_LOGCONFIG(TAG, "  CRC Enable: %s", TRUEFALSE(this->crc_enable_));
    }
    if (this->payload_length_ > 0) {
      ESP_LOGCONFIG(TAG, "  Payload Length: %" PRIu32, this->payload_length_);
    }
    if (!this->sync_value_.empty()) {
      char hex_buf[17];  // 8 bytes max = 16 hex chars + null
      ESP_LOGCONFIG(TAG, "  Sync Value: 0x%s",
                    format_hex_to(hex_buf, this->sync_value_.data(), this->sync_value_.size()));
    }
    if (this->preamble_size_ > 0 || this->preamble_detect_ > 0) {
      ESP_LOGCONFIG(TAG,
                    "  Preamble Polarity: 0x%X\n"
                    "  Preamble Size: %" PRIu16 "\n"
                    "  Preamble Detect: %" PRIu8 "\n"
                    "  Preamble Errors: %" PRIu8,
                    this->preamble_polarity_, this->preamble_size_, this->preamble_detect_, this->preamble_errors_);
    }
  }
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Configuring SX127x failed");
  }
}

}  // namespace esphome::sx127x
