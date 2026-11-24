#include "component.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#if __has_include("esphome/components/sx126x/sx126x.h")
#include "esphome/components/sx126x/sx126x.h"
#define WMBUS_RADIO_HAS_SX126X 1
#endif
#include "decode3of6.h"
#include "phy_crc.h"

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
  this->stats_last_report_ms_ = millis();
#ifdef WMBUS_RADIO_HAS_SX127X
  if (this->native_sx127x_ != nullptr) {
    if (this->native_sx127x_->is_failed()) {
      ESP_LOGE(TAG, "Native SX127x unavailable; refusing to start wM-Bus receiver");
      this->mark_failed();
      return;
    }
    this->native_sx127x_->set_wmbus_diagnostics(this->native_phy_diagnostics_);
    this->native_sx127x_->set_wmbus_stream_callback(
        &Radio::sx127x_stream_callback_, this);
    ESP_LOGW(TAG, "SX127x T1 FIFO streaming backend enabled (experimental; hardware RF unverified)");
    return;
  }
#endif
#ifdef WMBUS_RADIO_HAS_SX126X
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
#else
  ESP_LOGE(TAG, "No supported radio source included in this build");
  this->mark_failed();
#endif
}

void Radio::loop() {
  // One constant-size summary per minute; never log individual packets here.
  if (!this->native_phy_diagnostics_) return;
  const uint32_t now = millis();
  if (uint32_t(now - this->stats_last_report_ms_) < 60000u) return;
  this->stats_last_report_ms_ = now;
  const PhyStats &s = this->stats_;
  const PhyStats &p = this->stats_previous_;
  ESP_LOGI(TAG,
           "XMQ PHY 60s %s T1 valid=%u 3of6=%u length_bad=%u crc_bad=%u "
           "C1/A=%u C1/B=%u C1_crc_bad=%u C1_length_bad=%u "
           "XMQ heat=%u water=%u",
           this->native_sx127x_ ? "SX127x" : "SX126x",
           static_cast<unsigned>(s.t1_valid - p.t1_valid),
           static_cast<unsigned>(s.t1_symbol_bad - p.t1_symbol_bad),
           static_cast<unsigned>(s.t1_length_bad - p.t1_length_bad),
           static_cast<unsigned>(s.t1_crc_bad - p.t1_crc_bad),
           static_cast<unsigned>(s.c1_valid[0] - p.c1_valid[0]),
           static_cast<unsigned>(s.c1_valid[1] - p.c1_valid[1]),
           static_cast<unsigned>(s.c1_crc_bad - p.c1_crc_bad),
           static_cast<unsigned>(s.c1_length_bad - p.c1_length_bad),
           static_cast<unsigned>(s.xmq_decoded[0] - p.xmq_decoded[0]),
           static_cast<unsigned>(s.xmq_decoded[1] - p.xmq_decoded[1]));
  this->stats_previous_ = s;
}

#ifdef WMBUS_RADIO_HAS_SX127X
void Radio::on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) {
  // SX127x official packet mode supplies a bounded FIFO payload.
  // Feed it to the same strict T1/C1 PHY decoder as SX126x. No frame is
  // dispatched unless 3-of-6, length and DLL CRC checks all pass.
  // A normal long T1 or C1 telegram may exceed SX127x's 64-byte FIFO;
  // such frames require a future streaming RF backend.
  (void) snr;
  if (packet.empty() || packet.size() > MAX_NATIVE_PHY_SIZE || !std::isfinite(rssi))
    return;
  std::copy(packet.begin(), packet.end(), this->native_phy_.begin());
  this->native_phy_size_ = packet.size();
  this->decode_native_stream_(rssi);
  this->native_phy_size_ = 0;
}
#endif

// The standalone branch has exactly one application decoder: XMQ.
// Do not reintroduce Telegram::parseHeader or legacy meter dispatch here.
void Radio::dispatch_frame_(Frame &frame) {
  for (auto &handler : this->handlers_) handler(&frame);
  const bool handled = frame.handlers_count() != 0;
  if (!handled && this->frame_logging_) ESP_LOGD(TAG, "XMQ-only frame not claimed");

  // Analyzer URLs are made from CRC-verified, CRC-stripped DLL bytes only.
  // Publish no URL for invalid PHY frames. Never contact the website here.
  if (this->analyzer_links_ == 0 || (this->analyzer_links_ == 1 && handled))
    return;
  const auto &data = frame.data();
  if (data.size() < 11) return;

  // One report per meter identity per minute. On a busy receiver, hundreds
  // of foreign meters can transmit every minute; cap cache memory at 64 IDs.
  uint32_t identity = 2166136261u;
  for (size_t i = 1; i < 10; ++i) {
    identity = (identity ^ data[i]) * 16777619u;
  }
  const uint32_t now = millis();
  for (const auto &entry : this->analyzer_recent_) {
    if (entry.id == identity && entry.used &&
        uint32_t(now - entry.when) < 60000u)
      return;
  }
  auto &entry = this->analyzer_recent_[this->analyzer_next_++ % this->analyzer_recent_.size()];
  entry = {identity, now, true};
  const std::string url_hex = frame.as_hex();
  ESP_LOGI(TAG, "XMQ %s %s/%s RSSI=%d dBm analyzer: https://wmbusmeters.org/analyze/%s",
           handled ? "decoded" : "unhandled",
           frame.link_mode() == LinkMode::C1 ? "C1" : "T1",
           frame.format().c_str(), int(frame.rssi()), url_hex.c_str());
}

void Radio::sx127x_stream_callback_(void *context, const uint8_t *data,
                                       size_t length, float rssi) {
  if (context == nullptr || data == nullptr || length == 0) return;
  auto *radio = static_cast<Radio *>(context);
  radio->native_stream_(nullptr, 0, true, false, rssi);
  radio->native_stream_(data, length, false, false, rssi);
  radio->native_stream_(nullptr, 0, false, true, rssi);
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
      ESP_LOGW(TAG, "Native radio PHY overflow or null fragment; discarded");
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
#ifdef WMBUS_RADIO_HAS_SX127X
  auto record_sx127x = [this](sx127x::WmbusDecodeResult event) {
    if (this->native_sx127x_ != nullptr)
      this->native_sx127x_->note_wmbus_decode_result(event);
  };
#endif
  const size_t n = this->native_phy_size_;
  if (n < 3 || n > MAX_NATIVE_PHY_SIZE || !std::isfinite(rssi)) return;

  // The patched SX126x returns raw T1 3-of-6 bytes, or a C1 stream
  // starting with 54 CD (A) / 54 3D (B). It does NOT return a frame
  // already stripped of DLL CRCs. Never trust PHY completion as CRC.
  const bool c1 = n >= 3 && this->native_phy_[0] == 0x54 &&
                  (this->native_phy_[1] == 0xCD ||
                   this->native_phy_[1] == 0x3D);
  if (c1 && !this->native_t1_c1_b_) {
    if (this->frame_logging_) ESP_LOGD(TAG, "C1 rejected: T1-only source configured");
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
      ++this->stats_.c1_length_bad;
#ifdef WMBUS_RADIO_HAS_SX127X
      record_sx127x(sx127x::WmbusDecodeResult::BAD_LENGTH);
#endif
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
    if (!(format_b ? phy::strip_format_b(telegram)
                   : phy::strip_format_a(telegram))) {
      ++this->c1_crc_rejected_;
      ++this->stats_.c1_crc_bad;
#ifdef WMBUS_RADIO_HAS_SX127X
      record_sx127x(sx127x::WmbusDecodeResult::BAD_CRC);
#endif
      if (this->native_phy_diagnostics_ &&
          (this->c1_crc_rejected_ <= 12 ||
           this->c1_crc_rejected_ % 100 == 0)) {
        ESP_LOGI(TAG, "C1/%s CRC rejected total=%u", format,
                 static_cast<unsigned>(this->c1_crc_rejected_));
      }
      return;
    }
    if (format_b) { ++this->c1_b_verified_; ++this->stats_.c1_valid[1]; }
    else { ++this->c1_a_verified_; ++this->stats_.c1_valid[0]; }
    if (this->native_phy_diagnostics_ && this->frame_logging_) {
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
      ++this->stats_.t1_symbol_bad;
      if (this->frame_logging_) ESP_LOGD(TAG, "T1 3-of-6 symbol rejected");
#ifdef WMBUS_RADIO_HAS_SX127X
      record_sx127x(sx127x::WmbusDecodeResult::BAD_SYMBOL);
#endif
      return;
    }
    telegram = std::move(*decoded);
    const uint8_t l = telegram.front();
    if (l < 10) return;
    const size_t blocks = 1 + (static_cast<size_t>(l) - 9 + 15) / 16;
    const size_t expected_decoded = static_cast<size_t>(l) + 1 + 2 * blocks;
    if (telegram.size() != expected_decoded) {
      ++this->stats_.t1_length_bad;
#ifdef WMBUS_RADIO_HAS_SX127X
      record_sx127x(sx127x::WmbusDecodeResult::BAD_LENGTH);
#endif
      if (this->frame_logging_) ESP_LOGD(TAG, "T1 format A DLL length rejected");
      return;
    }
    if (!phy::strip_format_a(telegram)) {
      ++this->stats_.t1_crc_bad;
#ifdef WMBUS_RADIO_HAS_SX127X
      record_sx127x(sx127x::WmbusDecodeResult::BAD_CRC);
#endif
      if (this->frame_logging_) ESP_LOGD(TAG, "T1 format A DLL CRC rejected");
      return;
    }
  }

  if (telegram.size() < 11 ||
      static_cast<size_t>(telegram[0]) + 1 != telegram.size())
    return;
  // Direct PHY length check; no legacy checkWMBusFrame and no resync/parser.
  if (telegram[1] != 0x44 && telegram[1] != 0x46) return;
  // Saturate RSSI to the existing int8_t sensor API; do not substitute
  // premature average RSSI for Semtech's sync RSSI.
  const float bounded = std::max(-128.0f, std::min(0.0f, rssi));
  const int8_t signal = static_cast<int8_t>(std::lround(bounded));
#ifdef WMBUS_RADIO_HAS_SX127X
  if (c1)
    record_sx127x(format[0] == 'B'
                      ? sx127x::WmbusDecodeResult::C1B_VERIFIED
                      : sx127x::WmbusDecodeResult::C1A_VERIFIED);
  else
    record_sx127x(sx127x::WmbusDecodeResult::T1_VERIFIED);
#endif
  if (!c1) ++this->stats_.t1_valid;
  Frame frame(std::move(telegram), mode, signal, format);
  if (this->frame_logging_)
    ESP_LOGI(TAG, "Native %s verified %s/%s (%zu bytes, RSSI %d dBm)",
             this->native_sx127x_ ? "SX127x" : "SX126x",
             c1 ? "C1" : "T1", format, frame.data().size(), signal);
  this->dispatch_frame_(frame);
}

void Radio::add_frame_handler(std::function<void(Frame *)> &&callback) {
  this->handlers_.push_back(std::move(callback));
}

} // namespace wmbus_radio
} // namespace esphome
