#include "component.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include "esphome/components/sx126x/sx126x.h"
#include "decode3of6.h"

#define ASSERT(expr, expected, before_exit)                                    \
  {                                                                            \
    auto result = (expr);                                                      \
    if (!!result != expected) {                                                \
      ESP_LOGE(TAG, "Assertion failed: %s -> %d", #expr, result);              \
      before_exit;                                                             \
      return;                                                                  \
    }                                                                          \
  }

#define ASSERT_SETUP(expr) ASSERT(expr, 1, this->mark_failed())

namespace esphome {
namespace wmbus_radio {
static const char *TAG = "wmbus";

void Radio::setup() {
  if (this->native_sx126x_ == nullptr || this->native_sx126x_->is_failed()) {
    ESP_LOGE(TAG, "Native SX126x unavailable; refusing to start wM-Bus receiver");
    this->mark_failed();
    return;
  }
  this->native_sx126x_->set_combined_t1_c1_stream(this->native_t1_c1_b_);
  this->native_sx126x_->set_t1_stream_diagnostics(this->native_phy_diagnostics_);
  this->native_sx126x_->set_t1_stream_callback(&Radio::native_stream_callback_, this);
  this->native_sx126x_->set_mode_rx();
  if (this->native_sx126x_->is_failed()) {
    this->mark_failed();
    return;
  }
  ESP_LOGI(TAG, "Using native SX126x stream (T1%s), diagnostics=%s; no legacy receiver",
           this->native_t1_c1_b_ ? "+C1 A/B (experimental)" : "",
           this->native_phy_diagnostics_ ? "enabled" : "disabled");
}

void Radio::loop() {
  // Native SX126x invokes frame callbacks from its own ESPHome loop.
}

void Radio::dispatch_frame_(Frame &frame) {
  // Parse the validated frame header once for all meter subscriptions.
  // Preserve the original generic on_frame callbacks for every frame.
  Telegram header;
  const bool parsed = header.parseHeader(frame.data());
  for (auto &handler : this->handlers_)
    handler(&frame);

  if (parsed && !header.addresses.empty()) {
    for (auto &entry : this->meter_handlers_) {
      bool wildcard = false;
      if (doesTelegramMatchExpressions(header.addresses, entry.addresses, &wildcard))
        entry.callback(&frame);
    }
  } else {
    // Preserve previous meter behavior for a CRC-valid frame whose header
    // cannot be parsed by the lightweight prefilter. Individual handlers
    // still enforce their own mode and address checks.
    for (auto &entry : this->meter_handlers_)
      entry.callback(&frame);
  }

  if (frame.handlers_count()) {
    ESP_LOGI(TAG, "Telegram handled by %u handlers",
             static_cast<unsigned>(frame.handlers_count()));
    return;
  }

  ESP_LOGW(TAG, "Telegram not handled by any handler");
  if (!parsed || header.addresses.empty()) {
    ESP_LOGW(TAG, "No valid meter address in unhandled frame");
  } else {
    ESP_LOGW(TAG, "Unhandled meter address: %s", header.addresses.back().id.c_str());
  }
  ESP_LOGW(TAG, "%s",
           (std::string{"https://wmbusmeters.org/analyze/"} +
            frame.as_hex()).c_str());
}

void Radio::native_stream_callback_(void *context, const uint8_t *data,
                                    size_t length, bool start, bool end,
                                    float rssi) {
  if (context == nullptr) return;
  static_cast<Radio *>(context)->native_stream_(data, length, start, end, rssi);
}

void Radio::native_stream_(const uint8_t *data, size_t length,
                           bool start, bool end, float rssi) {
  // Always start fresh after the radio begins a new sync-word event.
  // A timeout on SX126x intentionally emits no end event.
  if (start) {
    this->native_phy_size_ = 0;
    this->native_phy_active_ = true;
    if (length != 0 || data != nullptr || end) {
      this->native_phy_active_ = false;
      return;
    }
  }
  if (!this->native_phy_active_) return;
  if (length != 0) {
    if (data == nullptr || length > MAX_NATIVE_PHY_SIZE - this->native_phy_size_) {
      ESP_LOGW(TAG, "Native SX126x PHY overflow or null fragment; discarded");
      this->native_phy_active_ = false;
      return;
    }
    std::copy_n(data, length, this->native_phy_.data() + this->native_phy_size_);
    this->native_phy_size_ += length;
  }
  if (end) {
    // No incomplete stream can reach any meter.
    this->native_phy_active_ = false;
    if (data != nullptr || length != 0 || this->native_phy_size_ < 3) return;
    this->decode_native_stream_(rssi);
    this->native_phy_size_ = 0;
  }
}

void Radio::decode_native_stream_(float rssi) {
  const size_t n = this->native_phy_size_;
  if (n < 3 || n > MAX_NATIVE_PHY_SIZE || !std::isfinite(rssi)) return;

  // The patched SX126x returns raw T1 3-of-6 bytes, or a C1 stream
  // starting with 54 CD (A) / 54 3D (B). It does NOT return a frame
  // already stripped of DLL CRCs. Never trust PHY completion as CRC.
  const bool c1 = n >= 3 && this->native_phy_[0] == 0x54 &&
                  (this->native_phy_[1] == 0xCD ||
                   this->native_phy_[1] == 0x3D);
  if (c1 && !this->native_t1_c1_b_) {
    ESP_LOGD(TAG, "C1 rejected: T1-only source configured");
    return;
  }
  const char *format = "A";
  const LinkMode mode = c1 ? LinkMode::C1 : LinkMode::T1;
  std::vector<uint8_t> telegram;
  if (c1) {
    // C1 mode marker: 54 CD = Format A; 54 3D = Format B.
    // Validate complete on-air length before removing any CRC bytes.
    const bool format_b = this->native_phy_[1] == 0x3D;
    format = format_b ? "B" : "A";
    telegram.assign(this->native_phy_.begin() + 2,
                    this->native_phy_.begin() + n);
    if (telegram.empty()) return;
    const size_t l = telegram.front();
    if (l < 10) return;
    // Format A L excludes CRC blocks; Format B L includes them.
    const size_t blocks = 1 + (l - 9 + 15) / 16;
    const size_t expected = format_b ? l + 1 : l + 1 + 2 * blocks;
    if (telegram.size() != expected) {
      ++this->c1_length_rejected_;
      if (this->native_phy_diagnostics_ &&
          (this->c1_length_rejected_ <= 12 ||
           this->c1_length_rejected_ % 100 == 0)) {
        ESP_LOGI(TAG, "C1/%s length rejected L=%u received=%u expected=%u total=%u",
                 format, static_cast<unsigned>(l),
                 static_cast<unsigned>(telegram.size()),
                 static_cast<unsigned>(expected),
                 static_cast<unsigned>(this->c1_length_rejected_));
      }
      return;
    }
    if (!(format_b ? trimCRCsFrameFormatB(telegram)
                   : trimCRCsFrameFormatA(telegram))) {
      ++this->c1_crc_rejected_;
      if (this->native_phy_diagnostics_ &&
          (this->c1_crc_rejected_ <= 12 ||
           this->c1_crc_rejected_ % 100 == 0)) {
        ESP_LOGI(TAG, "C1/%s CRC rejected total=%u", format,
                 static_cast<unsigned>(this->c1_crc_rejected_));
      }
      return;
    }
    if (format_b) ++this->c1_b_verified_;
    else ++this->c1_a_verified_;
    if (this->native_phy_diagnostics_) {
      ESP_LOGI(TAG, "C1 PHY adapter verified A=%u B=%u crc_bad=%u len_bad=%u",
               static_cast<unsigned>(this->c1_a_verified_),
               static_cast<unsigned>(this->c1_b_verified_),
               static_cast<unsigned>(this->c1_crc_rejected_),
               static_cast<unsigned>(this->c1_length_rejected_));
    }
  } else {
    std::vector<uint8_t> raw(this->native_phy_.begin(),
                             this->native_phy_.begin() + n);
    const auto decoded = decode3of6(raw);
    if (!decoded || decoded->empty()) {
      ESP_LOGD(TAG, "T1 3-of-6 symbol rejected");
      return;
    }
    telegram = std::move(*decoded);
    const uint8_t l = telegram.front();
    if (l < 10) return;
    const size_t blocks = 1 + (static_cast<size_t>(l) - 9 + 15) / 16;
    const size_t expected_decoded = static_cast<size_t>(l) + 1 + 2 * blocks;
    if (telegram.size() != expected_decoded ||
        !trimCRCsFrameFormatA(telegram)) {
      ESP_LOGD(TAG, "T1 format A DLL CRC/length rejected");
      return;
    }
  }

  if (telegram.size() < 11 ||
      static_cast<size_t>(telegram[0]) + 1 != telegram.size())
    return;
  // checkWMBusFrame only checks framing; CRC was already checked above.
  // Avoid its resync and length-adjustment behavior for untrusted input.
  size_t frame_length = 0;
  int payload_length = 0;
  int payload_offset = 0;
  const auto status = checkWMBusFrame(telegram, &frame_length,
                                      &payload_length, &payload_offset,
                                      false);
  if (status != FrameStatus::FullFrame || payload_offset != 1 ||
      frame_length != telegram.size())
    return;
  // Saturate RSSI to the existing int8_t sensor API; do not substitute
  // premature average RSSI for Semtech's sync RSSI.
  const float bounded = std::max(-128.0f, std::min(0.0f, rssi));
  const int8_t signal = static_cast<int8_t>(std::lround(bounded));
  Frame frame(std::move(telegram), mode, signal, format);
  ESP_LOGI(TAG, "Native SX126x verified %s/%s (%zu bytes, RSSI %d dBm)",
           toString(mode), format, frame.data().size(), signal);
  this->dispatch_frame_(frame);
}

void Radio::add_frame_handler(std::function<void(Frame *)> &&callback) {
  this->handlers_.push_back(std::move(callback));
}

void Radio::add_meter_frame_handler(std::vector<AddressExpression> addresses,
                                    std::function<void(Frame *)> &&callback) {
  this->meter_handlers_.push_back({std::move(addresses), std::move(callback)});
}

} // namespace wmbus_radio
} // namespace esphome
