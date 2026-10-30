#include "component.h"

#include <algorithm>
#include <memory>

#include "freertos/queue.h"
#include "freertos/task.h"
#include "pure_frame_header.h"
#include "pure_meter_dispatch.h"
#include "pure_aes_cbc.h"
#include "pure_aes_mbedtls.h"

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

// Link-time and runtime self-check for the ESP32 mbedTLS backend.
// This uses only public NIST test data and is independent of meter keys.
static bool pure_mbedtls_selfcheck() {
  const uint8_t key[16] = {0x2b,0x7e,0x15,0x16,0x28,0xae,0xd2,0xa6,
                            0xab,0xf7,0x15,0x88,0x09,0xcf,0x4f,0x3c};
  const uint8_t iv[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
  const uint8_t cipher[16] = {0x76,0x49,0xab,0xac,0x81,0x19,0xb2,0x46,
                               0xce,0xe9,0x8e,0x9b,0x12,0xe9,0x19,0x7d};
  const uint8_t expected[16] = {0x6b,0xc1,0xbe,0xe2,0x2e,0x40,0x9f,0x96,
                                 0xe9,0x3d,0x7e,0x11,0x73,0x93,0x17,0x2a};
  std::vector<uint8_t> clear;
  return wmbus_pure::decrypt_aes128_cbc(
      cipher, sizeof(cipher), key, iv, wmbus_pure::mbedtls_aes128_decrypt_block,
      nullptr, clear) == wmbus_pure::AesCbcStatus::Ok &&
      clear.size() == sizeof(expected) &&
      std::equal(clear.begin(), clear.end(), expected);
}

void Radio::setup() {
  ESP_LOGI(TAG, "Pure AES/mBedTLS self-check: %s",
           pure_mbedtls_selfcheck() ? "PASS" : "FAIL");
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
  // ESP-IDF FreeRTOS reports high-water marks in BYTES (not words).
  // Sample both stacks even when no radio telegram is in the queue.
  // This keeps the measurement independent of the packet-processing path.
  static TickType_t last_stack_log = 0;
  const TickType_t now = xTaskGetTickCount();
  if ((now - last_stack_log) >= pdMS_TO_TICKS(30000)) {
    last_stack_log = now;
    const UBaseType_t main_free = uxTaskGetStackHighWaterMark(nullptr);
    const UBaseType_t receiver_free =
        this->receiver_task_handle_ != nullptr
            ? uxTaskGetStackHighWaterMark(this->receiver_task_handle_)
            : 0;
    ESP_LOGI(TAG, "Stack low-water free: esphome_loop=%u B, radio_recv=%u B",
             static_cast<unsigned>(main_free),
             static_cast<unsigned>(receiver_free));
    if (main_free < 1024 || (this->receiver_task_handle_ != nullptr && receiver_free < 1024)) {
      ESP_LOGW(TAG, "Low task stack headroom (below 1024 bytes)");
    }
  }

  Packet *packet_ptr = nullptr;
  if (xQueueReceive(this->packet_queue_, &packet_ptr, 0) != pdPASS)
    return;

  // Ownership was released by receiver_task() after queueing.
  // Ensure the packet is freed even when frame conversion fails.
  std::unique_ptr<Packet> packet(packet_ptr);

  // ESP_LOGI(TAG, "Have RAW data from radio (%zu bytes)",
  //          packet->calculate_payload_size());

  auto frame = packet->convert_to_frame();

  if (!frame) {
    ESP_LOGD(TAG, "Failed to convert packet to frame");
    return;
  }

  ESP_LOGI(TAG, "Have data (%zu bytes) [RSSI: %ddBm, mode: %s %s]",
           frame->data().size(), frame->rssi(), toString(frame->link_mode()),
           frame->format().c_str());

  // Independent header reader is diagnostic-only during migration.
  // Keep the existing handlers and their sensor updates unchanged.
  wmbus_pure::Header pure_header{};
  const auto &payload = frame->data();
  const auto pure_status = wmbus_pure::parse_header(
      payload.data(), payload.size(), pure_header);
  if (pure_status == wmbus_pure::HeaderStatus::Ok) {
    ESP_LOGD(TAG, "Pure header meter_id=%08u manufacturer=%04X version=%02X type=%02X ci=%02X",
             static_cast<unsigned>(pure_header.meter_id),
             static_cast<unsigned>(pure_header.manufacturer),
             static_cast<unsigned>(pure_header.version),
             static_cast<unsigned>(pure_header.device_type),
             static_cast<unsigned>(pure_header.ci));
  } else {
    ESP_LOGD(TAG, "Pure header unsupported/invalid status=%u",
             static_cast<unsigned>(pure_status));
  }
  // Fast ID routing for registered meters; invalid headers use fallback.
  uint32_t selected = 0;
  for (auto &entry : this->meter_handlers_) {
    if (pure_status != wmbus_pure::HeaderStatus::Ok ||
        wmbus_pure::route_to_configured_meter(pure_header, entry.id.c_str())) {
      entry.callback(&frame.value());
      ++selected;
    }
  }
  for (auto &handler : this->handlers_)
    handler(&frame.value());
  static uint32_t dispatch_frames = 0, dispatch_matched = 0, dispatch_unmatched = 0;
  ++dispatch_frames;
  if (selected) ++dispatch_matched;
  else ++dispatch_unmatched;
  if (dispatch_frames % 100 == 1)
    ESP_LOGI(TAG, "Pure configured dispatch frames=%u matched=%u unmatched=%u registered=%u selected=%u",
             static_cast<unsigned>(dispatch_frames),
             static_cast<unsigned>(dispatch_matched),
             static_cast<unsigned>(dispatch_unmatched),
             static_cast<unsigned>(this->meter_handlers_.size()),
             static_cast<unsigned>(selected));
  
  if (frame->handlers_count())
    ESP_LOGI(TAG, "Telegram handled by %d handlers", frame->handlers_count());
  else {
    ESP_LOGW(TAG, "Telegram not handled by any handler");
    // Decode the primary DLL address without the legacy Telegram parser.
    // Unsupported frame variants remain explicit instead of guessing an ID.
    wmbus_pure::Header unmatched_header{};
    const auto unmatched_status = wmbus_pure::parse_header(
        frame->data().data(), frame->data().size(), unmatched_header);
    if (unmatched_status != wmbus_pure::HeaderStatus::Ok) {
      ESP_LOGW(TAG, "Independent header parser rejected frame (status=%u)",
               static_cast<unsigned>(unmatched_status));
    } else {
      ESP_LOGW(TAG, "Check if telegram with address %08u can be parsed on:",
               static_cast<unsigned>(unmatched_header.meter_id));
    }
    ESP_LOGW(TAG,
             (std::string{"https://wmbusmeters.org/analyze/"} + frame->as_hex())
                 .c_str());
  }
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

  if (!this->radio->read_in_task(packet->rx_data_ptr(), packet->rx_capacity(), 0)) {
    this->radio->restart_rx();
    return;
  }

  if (!packet->calculate_payload_size()) {
    this->radio->restart_rx();
    return;
  }

  if (!this->radio->read_in_task(packet->rx_data_ptr(), packet->rx_capacity(),  3)) {
    this->radio->restart_rx();
    return;
  }

  packet->set_rssi(this->radio->get_rssi());
  // Re-arm exactly once, after RSSI is captured and before queueing.
  this->radio->restart_rx();
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

void Radio::add_frame_handler(std::function<void(Frame *)> &&callback) {
  this->handlers_.push_back(std::move(callback));
}

void Radio::add_meter_handler(const std::string &meter_id,
                              std::function<void(Frame *)> &&callback) {
  if (!wmbus_pure::configured_id_is_exact(meter_id.c_str())) {
    ESP_LOGW(TAG, "Unsupported meter ID; using legacy fallback dispatch");
    this->add_frame_handler(std::move(callback));
    return;
  }
  this->meter_handlers_.push_back({meter_id, std::move(callback)});
}

} // namespace wmbus_radio
} // namespace esphome
