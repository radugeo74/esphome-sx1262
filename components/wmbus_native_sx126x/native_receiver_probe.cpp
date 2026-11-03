#include "native_receiver_probe.h"
#include "native_frame_boundary.h"
#include "native_verified_frame.h"
#include "c1_block_diagnostics.h"
#include "mkradio4a_plain.h"
#include "tpl_security_probe.h"
#include "native_meter_registry.h"
#include "c1_integrity.h"
#include "esphome/core/log.h"
#include <cmath>
#include <algorithm>
#ifdef WMBUS_NATIVE_XMQ_ENABLED
#include "xmq_decode_catalog.h"
#endif

namespace esphome {
namespace wmbus_native_sx126x {
static const char *const TAG = "wmbus.native_sx126x";

void NativeReceiverProbe::fire_verified_(uint32_t meter_id, float rssi, const std::vector<uint8_t> &bytes) {
  if (this->verified_triggers_.empty()) return;
  static constexpr char HEX[] = "0123456789ABCDEF";
  std::string telegram_hex;
  telegram_hex.reserve(bytes.size() * 2);
  for (uint8_t b : bytes) {
    telegram_hex.push_back(HEX[b >> 4]);
    telegram_hex.push_back(HEX[b & 0x0F]);
  }
  for (auto *trigger : this->verified_triggers_) {
    trigger->trigger(meter_id, rssi, telegram_hex, static_cast<uint32_t>(bytes.size()));
  }
}


// Only normalize the outgoing sensor measurement. The XMQ decoded values,
// driver calculations and radio packet remain untouched.
static float round_for_sensor(float value, sensor::Sensor *output) {
  if (output == nullptr || !std::isfinite(value))
    return value;
  const int decimals = output->get_accuracy_decimals();
  if (decimals < 0 || decimals > 6)
    return value;
  float scale = 1.0f;
  for (int i = 0; i < decimals; ++i)
    scale *= 10.0f;
  return std::round(value * scale) / scale;
}

#ifdef SX126X_EXPERIMENTAL_PROGRESS_PROBE
void NativeReceiverProbe::on_rx_fifo_progress(uint8_t fifo_count, uint8_t fifo_start) {
  // Rate-limit diagnostic to changes in FIFO status; no packet reads or publication.
  static uint8_t last_count = 0xFF;
  static uint8_t last_start = 0xFF;
  if (fifo_count != last_count || fifo_start != last_start) {
    ESP_LOGD(TAG, "SX126x live FIFO count=%u offset=%u (diagnostic only)",
             static_cast<unsigned>(fifo_count), static_cast<unsigned>(fifo_start));
    last_count = fifo_count;
    last_start = fifo_start;
  }
}
#endif

void NativeReceiverProbe::on_t1_stream(void *context, const uint8_t *data,
                                        size_t count, bool begin, bool end, float rssi_dbm) {
  auto *self = static_cast<NativeReceiverProbe *>(context);
  if (begin) self->t1_fifo_.begin_frame();
  if (count != 0 && self->t1_fifo_.state() == t1::RxState::Collecting)
    self->t1_fifo_.feed_bytes(data, count);
  if (end) {
    const auto state = self->t1_fifo_.end_frame();
    static uint32_t verified_frames = 0;
    static uint32_t rejected_frames = 0;
    // Diagnostic tap: the full 3-of-6 encoded FIFO stream, BEFORE DLL/CRC
    // acceptance. Do not dispatch these bytes to meter decoders or sensors.
    // Emit short chunks to avoid saturating the ESPHome logger's line buffer.
    // Combined RX prioritizes servicing the radio over verbose per-T1
    // hex dumps. Keep standalone T1 diagnostics unchanged.
    if (self->t1_raw_diagnostics_ && !self->combined_mode_) {
      const auto &raw = self->t1_fifo_.raw_encoded_bytes();
      ESP_LOGI(TAG, "T1 RAW frame state=%s encoded_bytes=%u dll_status=%u boundary=%u verify=%u",
               state == t1::RxState::Verified ? "verified" : "rejected",
               static_cast<unsigned>(raw.size()),
               static_cast<unsigned>(self->t1_fifo_.result().status),
               static_cast<unsigned>(self->t1_fifo_.result().boundary_status),
               static_cast<unsigned>(self->t1_fifo_.result().verify_status));
      static constexpr char HEX[] = "0123456789ABCDEF";
      for (size_t offset = 0; offset < raw.size(); offset += 32) {
        char encoded[65];
        const size_t n = std::min<size_t>(32, raw.size() - offset);
        for (size_t i = 0; i < n; ++i) {
          encoded[2 * i] = HEX[raw[offset + i] >> 4];
          encoded[2 * i + 1] = HEX[raw[offset + i] & 0x0F];
        }
        encoded[2 * n] = '\0';
        ESP_LOGD(TAG, "T1 RAW chunk offset=%u hex=%s",
                 static_cast<unsigned>(offset), encoded);
      }
    }
    if (state == t1::RxState::Verified) {
      ++self->t1_stats_.ok;
      ++verified_frames;
      ESP_LOGI(TAG, "T1 DLL VERIFIED count=%u rejected=%u",
               static_cast<unsigned>(verified_frames), static_cast<unsigned>(rejected_frames));
      const auto &frame = self->t1_fifo_.result().frame;
      const auto kind = registry::classify(self->t1_fifo_.result(), self->configured_meter_ids_);
      const auto security = wmbus_pure::tpl_security::inspect(frame.bytes.data(), frame.bytes.size());
      if (security.has_config_word) {
        ESP_LOGI(TAG, "T1 FIFO VERIFIED id=%08u len=%u rssi=%.1f dBm configured=%s tpl_encrypted=%s mode=%u",
                 static_cast<unsigned>(frame.header.meter_id),
                 static_cast<unsigned>(frame.bytes.size()), rssi_dbm,
                 kind == registry::Kind::Configured ? "yes" : "no",
                 wmbus_pure::tpl_security::label(security.state),
                 static_cast<unsigned>(security.mode));
      } else {
        ESP_LOGI(TAG, "T1 FIFO VERIFIED id=%08u len=%u rssi=%.1f dBm configured=%s tpl_encrypted=unknown ci=0x%02X",
                 static_cast<unsigned>(frame.header.meter_id),
                 static_cast<unsigned>(frame.bytes.size()), rssi_dbm,
                 kind == registry::Kind::Configured ? "yes" : "no",
                 static_cast<unsigned>(frame.header.ci));
      }
      self->fire_verified_(frame.header.meter_id, rssi_dbm, frame.bytes);
      if (kind == registry::Kind::Configured) {
        const auto meter_id = std::to_string(frame.header.meter_id);
        for (const auto &binding : self->rssi_sensors_) {
          if (binding.meter_id == meter_id && binding.output != nullptr)
            binding.output->publish_state(round_for_sensor(rssi_dbm, binding.output));
        }
#ifdef WMBUS_NATIVE_XMQ_ENABLED
        const auto id = std::to_string(frame.header.meter_id);
        const char *driver = nullptr;
        for (const auto &entry : self->meter_drivers_) {
          if (entry.id == id) { driver = entry.driver.c_str(); break; }
        }
        if (driver != nullptr) {
          xmq_portable::CalcValue values[24]{};
          xmq_portable::CalendarDate dates[2]{};
          xmq_portable::Capture captures[12]{};
          xmq_portable::DecodedPayload decoded{values, 24, 0, dates, 2, 0};
          const auto status = xmq_portable::decode_configured_plain_frame(
              frame.bytes.data(), frame.bytes.size(), id.c_str(), driver,
              captures, 12, decoded);
          if (status == xmq_portable::PureDecodeStatus::Ok) {
            for (const auto &binding : self->xmq_sensors_) {
              if (binding.meter_id != id || binding.output == nullptr) continue;
              for (size_t i = 0; i < decoded.value_count; ++i) {
                if (decoded.values[i].name != nullptr &&
                    binding.field == decoded.values[i].name &&
                    std::isfinite(decoded.values[i].value)) {
                  binding.output->publish_state(round_for_sensor(decoded.values[i].value, binding.output));
                  break;
                }
              }
            }
          }
          ESP_LOGI(TAG, "Native XMQ meter=%s driver=%s status=%u fields=%u (mapped sensors only)",
                   id.c_str(), driver, static_cast<unsigned>(status),
                   static_cast<unsigned>(decoded.value_count));
        }
#endif
      }
    } else {
      ++self->t1_stats_.rejected;
      ++rejected_frames;
      if (rejected_frames <= 10 || rejected_frames % 50 == 0)
        ESP_LOGW(TAG, "T1 DLL REJECTED count=%u verified=%u status=%u boundary=%u verify=%u bad_symbol=%u",
                 static_cast<unsigned>(rejected_frames), static_cast<unsigned>(verified_frames),
                 static_cast<unsigned>(self->t1_fifo_.result().status),
                 static_cast<unsigned>(self->t1_fifo_.result().boundary_status),
                 static_cast<unsigned>(self->t1_fifo_.result().verify_status),
                 self->t1_fifo_.result().first_bad_symbol == SIZE_MAX ? 0xFFFFFFFFU :
                 static_cast<unsigned>(self->t1_fifo_.result().first_bad_symbol));
    }
  }
}

// C1 laboratory path: only complete, CRC-verified frames are reported.
// No XMQ, RSSI sensor publication or trigger dispatch in this diagnostic mode.
void NativeReceiverProbe::on_c1_stream(void *context, const uint8_t *data,
                                       size_t count, bool begin, bool end, float rssi_dbm) {
  auto *self = static_cast<NativeReceiverProbe *>(context);
  if (begin) self->c1_encoded_frame_.clear();
  if (data != nullptr && count != 0) {
    if (self->c1_encoded_frame_.size() + count > 255) {
      self->c1_encoded_frame_.clear();
      ESP_LOGW(TAG, "C1 RAW buffer overflow");
      return;
    }
    self->c1_encoded_frame_.insert(self->c1_encoded_frame_.end(), data, data + count);
  }
  if (!end) return;
  const auto &raw = self->c1_encoded_frame_;
  const auto mode = self->c1_format_b_ ? boundary::Mode::C1B : boundary::Mode::C1A;
  const auto checked = verified::process(mode, raw.data(), raw.size());
  ESP_LOGI(TAG, "C1-%c FRAME rx=%u verified=%s boundary=%u verify=%u rssi=%.1f",
           self->c1_format_b_ ? 'B' : 'A', static_cast<unsigned>(raw.size()),
           checked.status == verified::Status::Ok ? "yes" : "no",
           static_cast<unsigned>(checked.boundary_status),
           static_cast<unsigned>(checked.verify_status), rssi_dbm);
  if (checked.status == verified::Status::Ok) {
    const auto security = wmbus_pure::tpl_security::inspect(
        checked.frame.bytes.data(), checked.frame.bytes.size());
    ESP_LOGI(TAG, "C1 VERIFIED id=%08u len=%u ci=0x%02X encrypted=%s (diagnostic only)",
             static_cast<unsigned>(checked.frame.header.meter_id),
             static_cast<unsigned>(checked.frame.bytes.size()),
             static_cast<unsigned>(checked.frame.header.ci),
             wmbus_pure::tpl_security::label(security.state));
  }
  if (self->t1_raw_diagnostics_) {
    static constexpr char HEX[] = "0123456789ABCDEF";
    for (size_t offset = 0; offset < raw.size(); offset += 32) {
      char chunk[65];
      const size_t n = std::min<size_t>(32, raw.size() - offset);
      for (size_t i = 0; i < n; ++i) {
        chunk[2*i] = HEX[raw[offset+i] >> 4];
        chunk[2*i+1] = HEX[raw[offset+i] & 15];
      }
      chunk[2*n] = 0;
      ESP_LOGD(TAG, "C1 RAW chunk offset=%u hex=%s",
               static_cast<unsigned>(offset), chunk);
    }
  }
}

// The common 54 3D sync is consumed by SX126x. FIFO data starts with
// 54 CD or 54 3D for C1; otherwise it contains T1 3-of-6 symbols.
// C1 remains diagnostic-only; T1 retains existing configured sensor updates.
void NativeReceiverProbe::on_combined_stream(void *context, const uint8_t *data,
                                              size_t count, bool begin, bool end, float rssi_dbm) {
  auto *self = static_cast<NativeReceiverProbe *>(context);
  if (begin) self->combined_encoded_frame_.clear();
  if (data != nullptr && count != 0) {
    if (self->combined_encoded_frame_.size() + count > 255) {
      self->combined_encoded_frame_.clear();
      ESP_LOGW(TAG, "Combined FIFO overflow: frame discarded");
      return;
    }
    self->combined_encoded_frame_.insert(self->combined_encoded_frame_.end(), data, data + count);
  }
  if (!end) return;
  const auto &raw = self->combined_encoded_frame_;
  if (raw.size() < 2) return;
  self->log_combined_stats_();
  const bool is_c1 = raw[0] == 0x54 && (raw[1] == 0xCD || raw[1] == 0x3D);
  if (!is_c1) {
    self->on_t1_stream(self, nullptr, 0, true, false, 0.0f);
    self->on_t1_stream(self, raw.data(), raw.size(), false, false, 0.0f);
    self->on_t1_stream(self, nullptr, 0, false, true, rssi_dbm);
    return;
  }
  const auto mode = raw[1] == 0xCD ? boundary::Mode::C1A : boundary::Mode::C1B;
  const auto checked = verified::process(mode, raw.data() + 2, raw.size() - 2);
  auto &stats = mode == boundary::Mode::C1A ? self->c1a_stats_ : self->c1b_stats_;
  if (checked.status == verified::Status::Ok) ++stats.ok;
  else ++stats.rejected;
  ESP_LOGI(TAG, "COMBINED C1-%c rx=%u verified=%s boundary=%u verify=%u rssi=%.1f (diagnostic only)",
           raw[1] == 0xCD ? 'A' : 'B', static_cast<unsigned>(raw.size()),
           checked.status == verified::Status::Ok ? "yes" : "no",
           static_cast<unsigned>(checked.boundary_status),
           static_cast<unsigned>(checked.verify_status), rssi_dbm);
  // Bounded local diagnostics for CRC failures in common-sync C1 reception.
  // Never include real meter samples or IDs in repository fixtures.
  if (self->t1_raw_diagnostics_ && checked.status != verified::Status::Ok) {
    static uint32_t samples = 0;
    if (samples++ < 12) {
      static constexpr char HEX[] = "0123456789ABCDEF";
      const size_t n = std::min<size_t>(raw.size(), 24);
      char prefix[49];
      for (size_t i = 0; i < n; ++i) {
        prefix[2 * i] = HEX[raw[i] >> 4];
        prefix[2 * i + 1] = HEX[raw[i] & 0x0F];
      }
      prefix[2 * n] = 0;
      const auto crc = c1diagnostic::inspect(
          raw.data() + 2, raw.size() - 2,
          mode == boundary::Mode::C1A ? wmbus_pure::CrcFrameFormat::A : wmbus_pure::CrcFrameFormat::B);
      ESP_LOGD(TAG, "COMBINED C1 CRC probe prefix=%s rx=%u block=%u offset=%u failure=%u received=%04X expected=%04X (local only)",
               prefix, static_cast<unsigned>(raw.size()),
               static_cast<unsigned>(crc.block), static_cast<unsigned>(crc.offset),
               static_cast<unsigned>(crc.failure),
               static_cast<unsigned>(crc.received), static_cast<unsigned>(crc.calculated));
    }
  }
  if (checked.status == verified::Status::Ok) {
    const auto security = wmbus_pure::tpl_security::inspect(
        checked.frame.bytes.data(), checked.frame.bytes.size());
    const auto kind = registry::classify(checked, self->configured_meter_ids_);
    ESP_LOGI(TAG, "COMBINED C1 VERIFIED id=%08u ci=0x%02X configured=%s encrypted=%s",
             static_cast<unsigned>(checked.frame.header.meter_id),
             static_cast<unsigned>(checked.frame.header.ci),
             kind == registry::Kind::Configured ? "yes" : "no",
             wmbus_pure::tpl_security::label(security.state));
    // Trial of the pinned XMQ mkradio4a cleartext fields for configured
    // meters ONLY. No AES, no legacy decoding and no sensor publication.
    // A successful DLL CRC does not imply that the TPL payload is plaintext.
    if (kind == registry::Kind::Configured) {
      bool mkradio4_family = false;
      const auto id = std::to_string(checked.frame.header.meter_id);
      for (const auto &entry : self->meter_drivers_) {
        if (entry.id == id && (entry.driver == "mkradio4" || entry.driver == "mkradio4a")) {
          mkradio4_family = true;
          break;
        }
      }
      if (mkradio4_family) {
        xmq_portable::mkradio4a::Reading reading{};
        const auto status = xmq_portable::mkradio4a::decode(
            checked.frame.bytes.data(), checked.frame.bytes.size(), reading);
        if (status == xmq_portable::mkradio4a::Status::Ok) {
          ESP_LOGI(TAG, "C1 mkradio4a XMQ candidate target_m3=%.1f target_date=%04d-%02d-%02d (diagnostic, no publish)",
                   reading.target_m3, reading.year, reading.month, reading.day);
        } else {
          static uint32_t unsuccessful = 0;
          if (++unsuccessful <= 3 || unsuccessful % 100 == 0)
            ESP_LOGD(TAG, "C1 mkradio4a XMQ unsupported plaintext layout status=%u (no publish)",
                     static_cast<unsigned>(status));
        }
      }
    }
    // Mirror the existing T1 Telegram log for EVERY CRC-verified C1 frame,
    // including unconfigured meters. Display only: no sensor publication,
    // decryption, or trigger emission. Frame bytes have DLL CRC stripped.
    static constexpr char HEX[] = "0123456789ABCDEF";
    const auto &bytes = checked.frame.bytes;
    std::string telegram;
    telegram.reserve(bytes.size() * 2);
    for (uint8_t value : bytes) {
      telegram.push_back(HEX[value >> 4]);
      telegram.push_back(HEX[value & 0x0F]);
    }
    // Append an encryption marker only when the supported TPL security
    // configuration positively identifies encryption (modes 4 or 5).
    // Unknown and manufacturer-specific formats are never guessed.
    if (security.state == wmbus_pure::tpl_security::State::Encrypted) {
      ESP_LOGD(TAG, "C1-%c RSSI: %.1f dBm Telegram: %s (%u) encrypted=yes",
               mode == boundary::Mode::C1A ? 'A' : 'B', rssi_dbm,
               telegram.c_str(), static_cast<unsigned>(bytes.size()));
    } else {
      ESP_LOGD(TAG, "C1-%c RSSI: %.1f dBm Telegram: %s (%u)",
               mode == boundary::Mode::C1A ? 'A' : 'B', rssi_dbm,
               telegram.c_str(), static_cast<unsigned>(bytes.size()));
    }
  }
}

void NativeReceiverProbe::log_combined_stats_() {
  if (!this->combined_mode_) return;
  const uint32_t now = millis();
  if (this->combined_stats_last_ms_ == 0) {
    this->combined_stats_last_ms_ = now;
    return;
  }
  if (static_cast<uint32_t>(now - this->combined_stats_last_ms_) < 60000U) return;
  this->combined_stats_last_ms_ = now;
  ESP_LOGI(TAG, "COMBINED RX stats T1 ok=%u bad=%u C1A ok=%u bad=%u C1B ok=%u bad=%u (since boot)",
           static_cast<unsigned>(this->t1_stats_.ok),
           static_cast<unsigned>(this->t1_stats_.rejected),
           static_cast<unsigned>(this->c1a_stats_.ok),
           static_cast<unsigned>(this->c1a_stats_.rejected),
           static_cast<unsigned>(this->c1b_stats_.ok),
           static_cast<unsigned>(this->c1b_stats_.rejected));
}

void NativeReceiverProbe::setup() {
  if (this->radio_ == nullptr) {
    ESP_LOGE(TAG, "Missing official SX126x instance");
    this->mark_failed();
    return;
  }
  this->radio_->register_listener(this);
  if (this->combined_mode_) {
    this->radio_->set_c1_stream_format(false, false);
    this->radio_->set_combined_t1_c1_stream(true);
    this->radio_->set_t1_stream_diagnostics(this->t1_raw_diagnostics_);
    this->radio_->set_t1_stream_callback(&NativeReceiverProbe::on_combined_stream, this);
    this->radio_->set_mode_rx();
    ESP_LOGI(TAG, "Combined T1/C1-A/C1-B RX enabled (C1 diagnostic only)");
  }
  if (!this->t1_mode_ && !this->combined_mode_) {
    this->radio_->set_c1_stream_format(true, this->c1_format_b_);
    this->radio_->set_t1_stream_diagnostics(this->t1_raw_diagnostics_);
    this->radio_->set_t1_stream_callback(&NativeReceiverProbe::on_c1_stream, this);
    this->radio_->set_mode_rx();
    ESP_LOGI(TAG, "C1-%c progressive diagnostic receiver enabled", this->c1_format_b_ ? 'B' : 'A');
  }
  if (this->t1_mode_) {
    this->radio_->set_t1_stream_diagnostics(this->t1_raw_diagnostics_);
    this->radio_->set_t1_stream_callback(&NativeReceiverProbe::on_t1_stream, this);
    this->radio_->set_mode_rx();
    ESP_LOGI(TAG, "Official SX126x T1 progressive FIFO receiver enabled (diagnostic only)");
  }
  if (this->fifo_progress_probe_) {
#ifdef SX126X_EXPERIMENTAL_PROGRESS_PROBE
    this->radio_->set_rx_progress_observer(&NativeReceiverProbe::on_rx_fifo_progress);
    ESP_LOGI(TAG, "SX126x in-flight FIFO progress probe enabled (diagnostic only)");
#else
    ESP_LOGE(TAG, "fifo_progress_probe requires experimental patched SX126x; probe disabled");
    this->mark_failed();
    return;
#endif
  }
  ESP_LOGI(TAG, "Native SX126x packet probe attached; diagnostic only, no decoding or publishing");
}

void NativeReceiverProbe::on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) {
  const wmbus_radio::native_radio::PacketView view{
    packet.data(), packet.size(), rssi, snr
  };
  const auto status = wmbus_radio::native_radio::validate(view);
  if (status != wmbus_radio::native_radio::InputStatus::Ok) {
    ++this->rejected_;
    ESP_LOGW(TAG, "Native packet ignored status=%u size=%u rejected=%u",
             static_cast<unsigned>(status), static_cast<unsigned>(packet.size()),
             static_cast<unsigned>(this->rejected_));
    return;
  }
  ++this->accepted_;
  // C1 uses sync [0x54,0x3D] (B) or [0x54,0xCD] (A).
  // T1 requires a separately validated radio sync configuration.
  // Those sync bytes are absent from native RX FIFO payload. Reconstitute a
  // bounded radio frame for structural diagnostics only; CRC is NOT checked.
  // Do not send it to meter handlers or publish measurements.
  const auto mode = this->t1_mode_ ? boundary::Mode::T1 :
      (this->c1_format_b_ ? boundary::Mode::C1B : boundary::Mode::C1A);
  const auto framed = boundary::convert(mode, packet.data(), packet.size());
  ESP_LOGI(TAG, "Native %s boundary status=%u dll_size=%u (unverified)",
           this->t1_mode_ ? "T1" : (this->c1_format_b_ ? "C1-B" : "C1-A"),
           static_cast<unsigned>(framed.status),
           static_cast<unsigned>(framed.decoded_dll_with_crc.size()));
  if (framed.status == boundary::Status::Framed && !this->t1_mode_ && this->c1_format_b_) {
    const auto integrity = c1::probe_format_b_crc(
        framed.decoded_dll_with_crc.data(),
        framed.decoded_dll_with_crc.size());
    ESP_LOGI(TAG, "C1-B CRC diagnostic status=%u (no meter publication)",
             static_cast<unsigned>(integrity));
  }
  // Verify DLL independently; diagnostics only. Never route an unverified frame.
  const auto check = verified::process(mode, packet.data(), packet.size());
  if (check.status == verified::Status::Ok) {
    const auto kind = registry::classify(check, this->configured_meter_ids_);
    ESP_LOGI(TAG, "Native VERIFIED id=%08u mode=%s dll_size=%u configured=%s (diagnostic only)",
             static_cast<unsigned>(check.frame.header.meter_id),
             this->t1_mode_ ? "T1" : (this->c1_format_b_ ? "C1-B" : "C1-A"),
             static_cast<unsigned>(check.frame.bytes.size()), kind == registry::Kind::Configured ? "yes" : "no");
  } else {
    ESP_LOGD(TAG, "Native NOT_VERIFIED boundary=%u crc_or_header=%u (no publication)",
             static_cast<unsigned>(check.boundary_status),
             static_cast<unsigned>(check.verify_status));
  }
  // Do not infer whether sync bytes are present or that CRC is valid.
  // A packet from SX126x::on_packet is not necessarily a complete wM-Bus frame.
  ESP_LOGI(TAG, "Native RX size=%u rssi=%.1f snr=%.1f first=%02X %02X %02X accepted=%u",
           static_cast<unsigned>(packet.size()), rssi, snr,
           packet[0], packet.size() > 1 ? packet[1] : 0,
           packet.size() > 2 ? packet[2] : 0,
           static_cast<unsigned>(this->accepted_));
}
}  // namespace wmbus_native_sx126x
}  // namespace esphome
