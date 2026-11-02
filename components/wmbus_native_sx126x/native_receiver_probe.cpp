#include "native_receiver_probe.h"
#include "native_frame_boundary.h"
#include "native_verified_frame.h"
#include "native_meter_registry.h"
#include "c1_integrity.h"
#include "esphome/core/log.h"
#include <cmath>
#ifdef WMBUS_NATIVE_XMQ_ENABLED
#include "xmq_decode_catalog.h"
#endif

namespace esphome {
namespace wmbus_native_sx126x {
static const char *const TAG = "wmbus.native_sx126x";

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
                                        size_t count, bool begin, bool end) {
  auto *self = static_cast<NativeReceiverProbe *>(context);
  if (begin) self->t1_fifo_.begin_frame();
  if (count != 0 && self->t1_fifo_.state() == t1::RxState::Collecting)
    self->t1_fifo_.feed_bytes(data, count);
  if (end) {
    const auto state = self->t1_fifo_.end_frame();
    if (state == t1::RxState::Verified) {
      const auto &frame = self->t1_fifo_.result().frame;
      const auto kind = registry::classify(self->t1_fifo_.result(), self->configured_meter_ids_);
      ESP_LOGI(TAG, "T1 FIFO VERIFIED len=%u configured=%s",
               static_cast<unsigned>(frame.bytes.size()),
               kind == registry::Kind::Configured ? "yes" : "no");
      if (kind == registry::Kind::Configured) {
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
                  binding.output->publish_state(decoded.values[i].value);
                  break;
                }
              }
            }
          }
          ESP_LOGI(TAG, "Native XMQ status=%u fields=%u (mapped sensors only)",
                   static_cast<unsigned>(status),
                   static_cast<unsigned>(decoded.value_count));
        }
#endif
      }
    }
  }
}

void NativeReceiverProbe::setup() {
  if (this->radio_ == nullptr) {
    ESP_LOGE(TAG, "Missing official SX126x instance");
    this->mark_failed();
    return;
  }
  this->radio_->register_listener(this);
  if (this->t1_mode_) {
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
  ESP_LOGI(TAG, "Native SX126x receiver attached; configured verified frames may update mapped XMQ sensors");
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
    ESP_LOGI(TAG, "Native VERIFIED mode=%s dll_size=%u configured=%s (diagnostic only)",
             this->t1_mode_ ? "T1" : (this->c1_format_b_ ? "C1-B" : "C1-A"),
             static_cast<unsigned>(check.frame.bytes.size()), kind == registry::Kind::Configured ? "yes" : "no");
  } else {
    ESP_LOGD(TAG, "Native NOT_VERIFIED boundary=%u crc_or_header=%u (no publication)",
             static_cast<unsigned>(check.boundary_status),
             static_cast<unsigned>(check.verify_status));
  }
  // Do not infer whether sync bytes are present or that CRC is valid.
  // A packet from SX126x::on_packet is not necessarily a complete wM-Bus frame.
  ESP_LOGI(TAG, "Native RX size=%u rssi=%.1f snr=%.1f accepted=%u",
           static_cast<unsigned>(packet.size()), rssi, snr,
           static_cast<unsigned>(this->accepted_));
}
}  // namespace wmbus_native_sx126x
}  // namespace esphome
