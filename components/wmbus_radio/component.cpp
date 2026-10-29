#include "component.h"

#include <memory>

#include "freertos/queue.h"
#include "freertos/task.h"

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
    vQueueDelete(this->packet_queue_);
    this->packet_queue_ = nullptr;
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
    const uint32_t queued = this->packets_queued_.load(std::memory_order_relaxed);
    const uint32_t dequeued = this->packets_dequeued_.load(std::memory_order_relaxed);
    ESP_LOGI(TAG,
             "Packet lifetime: created=%lu read_fail=%lu queued=%lu drop=%lu "
             "dequeued=%lu invalid=%lu valid=%lu pending=%lu",
             static_cast<unsigned long>(this->packets_created_.load(std::memory_order_relaxed)),
             static_cast<unsigned long>(this->packets_read_failed_.load(std::memory_order_relaxed)),
             static_cast<unsigned long>(this->packets_queued_.load(std::memory_order_relaxed)),
             static_cast<unsigned long>(this->packets_queue_dropped_.load(std::memory_order_relaxed)),
             static_cast<unsigned long>(this->packets_dequeued_.load(std::memory_order_relaxed)),
             static_cast<unsigned long>(this->packets_invalid_.load(std::memory_order_relaxed)),
             static_cast<unsigned long>(this->packets_valid_.load(std::memory_order_relaxed)),
             static_cast<unsigned long>(queued >= dequeued ? queued - dequeued : 0));
    // The counters are updated on separate cores, so the snapshot may be
    // transiently inconsistent; flag sustained inconsistencies only.
    const uint32_t created = this->packets_created_.load(std::memory_order_relaxed);
    const uint32_t failed = this->packets_read_failed_.load(std::memory_order_relaxed);
    const uint32_t dropped = this->packets_queue_dropped_.load(std::memory_order_relaxed);
    const uint32_t invalid = this->packets_invalid_.load(std::memory_order_relaxed);
    const uint32_t valid = this->packets_valid_.load(std::memory_order_relaxed);
    // A successfully queued packet is either pending, being processed,
    // classified invalid or classified valid. Never infer a memory leak solely
    // from a single cross-core snapshot.
    if (dequeued > queued + 1U || valid > dequeued + 1U ||
        invalid > dequeued + 1U || failed > created + 1U ||
        dropped > created + 1U) {
      ESP_LOGW(TAG, "Packet counters inconsistent; inspect repeated samples");
    }
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
  this->packets_dequeued_.fetch_add(1, std::memory_order_relaxed);

  // ESP_LOGI(TAG, "Have RAW data from radio (%zu bytes)",
  //          packet->calculate_payload_size());

  auto frame = packet->convert_to_frame();

  if (!frame) {
    this->packets_invalid_.fetch_add(1, std::memory_order_relaxed);
    ESP_LOGD(TAG, "Failed to convert packet to frame");
    return;
  }

  this->packets_valid_.fetch_add(1, std::memory_order_relaxed);
  ESP_LOGI(TAG, "Have data (%zu bytes) [RSSI: %ddBm, mode: %s %s]",
           frame->data().size(), frame->rssi(), toString(frame->link_mode()),
           frame->format().c_str());

  for (auto &handler : this->handlers_)
    handler(&frame.value());
  
  if (frame->handlers_count())
    ESP_LOGI(TAG, "Telegram handled by %d handlers", frame->handlers_count());
  else {
    ESP_LOGW(TAG, "Telegram not handled by any handler");
    Telegram t;
    const bool parsed = t.parseHeader(frame->data());
    if (!parsed || t.addresses.empty()) {
      ESP_LOGW(TAG, "Telegram header could not be parsed or has no address");
    } else {
      ESP_LOGW(TAG, "Check if telegram with address %s can be parsed on:",
               t.addresses.back().id.c_str());
    }
    ESP_LOGW(TAG,
             (std::string{"https://wmbusmeters.org/analyze/"} + frame->as_hex())
                 .c_str());
  }
}

void Radio::wakeup_receiver_task_from_isr(TaskHandle_t *arg) {
  if (arg == nullptr || *arg == nullptr)
    return;
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(*arg, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken == pdTRUE)
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void Radio::receive_frame() {
  if (!ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(60000))) {
    this->radio->restart_rx();
    return;
  }

  auto packet = std::make_unique<Packet>();
  this->packets_created_.fetch_add(1, std::memory_order_relaxed);

  if (!this->radio->read_in_task(packet->rx_data_ptr(), packet->rx_capacity(), 0)) {
    this->packets_read_failed_.fetch_add(1, std::memory_order_relaxed);
    this->radio->restart_rx();
    return;
  }

  if (!packet->calculate_payload_size()) {
    this->packets_read_failed_.fetch_add(1, std::memory_order_relaxed);
    this->radio->restart_rx();
    return;
  }

  if (!this->radio->read_in_task(packet->rx_data_ptr(), packet->rx_capacity(),  3)) {
    this->packets_read_failed_.fetch_add(1, std::memory_order_relaxed);
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
    this->packets_queued_.fetch_add(1, std::memory_order_relaxed);
  } else {
    this->packets_queue_dropped_.fetch_add(1, std::memory_order_relaxed);
    ESP_LOGW(TAG, "Queue send failed");
  }
}

void Radio::receiver_task(Radio *arg) {
  while (true)
    arg->receive_frame();
}

void Radio::add_frame_handler(std::function<void(Frame *)> &&callback) {
  this->handlers_.push_back(std::move(callback));
}

} // namespace wmbus_radio
} // namespace esphome
