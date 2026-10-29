#pragma once

#include <functional>
#include <atomic>
#include <cstdint>

#include "freertos/FreeRTOS.h"

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"

#include "esphome/components/spi/spi.h"
#include "esphome/components/wmbus_common/wmbus.h"

#include "packet.h"
#include "transceiver.h"

namespace esphome {
namespace wmbus_radio {

class Radio : public Component {
public:
  void set_radio(RadioTransceiver *radio) { this->radio = radio; };

  void setup() override;
  void loop() override;
  void receive_frame();

  void add_frame_handler(std::function<void(Frame *)> &&callback);

protected:
  static void wakeup_receiver_task_from_isr(TaskHandle_t *arg);
  static void receiver_task(Radio *arg);

  RadioTransceiver *radio{nullptr};
  TaskHandle_t receiver_task_handle_{nullptr};
  QueueHandle_t packet_queue_{nullptr};

  // Cross-core diagnostics. Relaxed atomics are sufficient for independent counts.
  std::atomic<uint32_t> packets_created_{0};
  std::atomic<uint32_t> packets_read_failed_{0};
  std::atomic<uint32_t> packets_queued_{0};
  std::atomic<uint32_t> packets_queue_dropped_{0};
  std::atomic<uint32_t> packets_dequeued_{0};
  std::atomic<uint32_t> packets_invalid_{0};
  std::atomic<uint32_t> packets_valid_{0};

  std::vector<std::function<void(Frame *)>> handlers_;
};
} // namespace wmbus_radio
} // namespace esphome
