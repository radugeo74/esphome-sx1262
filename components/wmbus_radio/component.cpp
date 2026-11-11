#include "component.h"

#include "freertos/queue.h"
#include "freertos/task.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#ifdef USE_WMBUS_NATIVE_SX126X
#include "esphome/components/sx126x/sx126x.h"
#include "decode3of6.h"
#endif

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
#ifdef USE_WMBUS_NATIVE_SX126X
  if (this->native_sx126x_ != nullptr) {
    if (this->native_sx126x_->is_failed()) {
      ESP_LOGE(TAG, "Native SX126x failed; refusing to start wM-Bus receiver");
      this->mark_failed();
      return;
    }
    // No FreeRTOS receiver task, second CS, IRQ or SX1262 SPI instance.
    this->native_sx126x_->set_combined_t1_c1_stream(this->native_t1_c1_b_);
    this->native_sx126x_->set_t1_stream_callback(
        &Radio::native_stream_callback_, this);
    // SX126x::setup has already run (PROCESSOR priority): re-arm with
    // sync-word streaming now installed, before publication is enabled.
    this->native_sx126x_->set_mode_rx();
    if (this->native_sx126x_->is_failed()) {
      this->mark_failed();
      return;
    }
    ESP_LOGI(TAG, "Using patched SX126x stream (T1%s), legacy task disabled",
             this->native_t1_c1_b_ ? "+C1 format B" : "");
    return;
  }
#endif
  // Create queue used to pass received packets to the main ESPHome loop.
  this->packet_queue_ = xQueueCreate(3, sizeof(Packet *));

  if (this->packet_queue_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create radio packet queue");
    this->mark_failed();
    return;
  }

  BaseType_t task_result;

#if portNUM_PROCESSORS > 1
  // ESP32 / ESP32-S3 dual-core:
  // keep the receiver on core 1 and give it enough stack.
  task_result = xTaskCreatePinnedToCore(
      (TaskFunction_t) this->receiver_task,
      "radio_recv",
      8 * 1024,
      this,
      24,
      &(this->receiver_task_handle_),
      1
  );
#else
  // Single-core ESP32 variants.
  task_result = xTaskCreate(
      (TaskFunction_t) this->receiver_task,
      "radio_recv",
      8 * 1024,
      this,
      24,
      &(this->receiver_task_handle_)
  );
#endif

  if (task_result != pdPASS) {
    ESP_LOGE(TAG, "Failed to create radio receiver task");
    this->mark_failed();
    return;
  }

  ESP_LOGI(TAG, "Receiver task created [%p]", this->receiver_task_handle_);

  this->radio->attach_data_interrupt(
      Radio::wakeup_receiver_task_from_isr,
      &(this->receiver_task_handle_)
  );
}

void Radio::loop() {
#ifdef USE_WMBUS_NATIVE_SX126X
  // Native SX126x invokes our callback from its ESPHome loop; data has
  // already been delivered to handlers synchronously after CRC verification.
  if (this->native_sx126x_ != nullptr) return;
#endif
  Packet *p;
  if (xQueueReceive(this->packet_queue_, &p, 0) != pdPASS)
    return;

  // ESP_LOGI(TAG, "Have RAW data from radio (%zu bytes)",
  //          p->calculate_payload_size());

  auto frame = p->convert_to_frame();

  if (!frame) {
    ESP_LOGD(TAG, "Failed to convert packet to frame");
    return;
  }

  ESP_LOGI(TAG, "Have data (%zu bytes) [RSSI: %ddBm, mode: %s %s]",
           frame->data().size(), frame->rssi(), toString(frame->link_mode()),
           frame->format().c_str());

  this->dispatch_frame_(frame.value());
}

void Radio::dispatch_frame_(Frame &frame) {
  for (auto &handler : this->handlers_)
    handler(&frame);

  if (frame.handlers_count()) {
    ESP_LOGI(TAG, "Telegram handled by %u handlers",
             static_cast<unsigned>(frame.handlers_count()));
    return;
  }

  ESP_LOGW(TAG, "Telegram not handled by any handler");
  Telegram t;
  const bool parsed = t.parseHeader(frame.data());
  if (!parsed || t.addresses.empty()) {
    ESP_LOGW(TAG, "No valid meter address in unhandled frame");
  } else {
    ESP_LOGW(TAG, "Unhandled meter address: %s", t.addresses.back().id.c_str());
  }
  ESP_LOGW(TAG, "%s",
           (std::string{"https://wmbusmeters.org/analyze/"} +
            frame.as_hex()).c_str());
}

void Radio::wakeup_receiver_task_from_isr(TaskHandle_t *arg) {
  BaseType_t xHigherPriorityTaskWoken;
  vTaskNotifyGiveFromISR(*arg, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void Radio::receive_frame() {
  if (!ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(60000))) {
    this->radio->restart_rx();
    return;
  }

  auto packet = std::make_unique<Packet>();

  if (!this->radio->read_in_task(packet->rx_data_ptr(), packet->rx_capacity(), 0))
    return;

  if (!packet->calculate_payload_size())
    return;

  if (!this->radio->read_in_task(packet->rx_data_ptr(), packet->rx_capacity(),  3))
    return;

  packet->set_rssi(this->radio->get_rssi());
  auto packet_ptr = packet.get();

  if (xQueueSend(this->packet_queue_, &packet_ptr, 0) == pdTRUE) {
    ESP_LOGV(TAG, "Queue items: %zu",
             uxQueueMessagesWaiting(this->packet_queue_));
    ESP_LOGV(TAG, "Queue send success");
    packet.release();
  } else
    ESP_LOGW(TAG, "Queue send failed");
}

void Radio::receiver_task(Radio *arg) {
  int counter = 0;
  while (true)
    arg->receive_frame();
}


#ifdef USE_WMBUS_NATIVE_SX126X
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
    // Combined T1/C1 PHY currently supports format B. Format A would
    // require independently verified on-air RX semantics and a mode gate.
    if (this->native_phy_[1] != 0x3D) return;
    format = "B";
    telegram.assign(this->native_phy_.begin() + 2,
                    this->native_phy_.begin() + n);
    if (telegram.empty() || telegram[0] < 10 ||
        static_cast<size_t>(telegram[0]) + 1 != telegram.size() ||
        !trimCRCsFrameFormatB(telegram)) {
      ESP_LOGD(TAG, "C1 format B DLL CRC/length rejected");
      return;
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
#endif

void Radio::add_frame_handler(std::function<void(Frame *)> &&callback) {
  this->handlers_.push_back(std::move(callback));
}

} // namespace wmbus_radio
} // namespace esphome
