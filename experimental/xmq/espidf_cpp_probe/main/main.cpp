#include <cmath>
#include <cstdint>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "record_decoder.h"

static const char *TAG = "xmq_cpp_probe";

extern "C" void app_main(void) {
  using namespace xmq_portable;
  const uint8_t raw[2] = {0xBD, 0x08};
  Record record{};
  const Status result = decode_record("0265", 4, raw, 2, record);
  if (result != Status::Ok || record.unit != Unit::TemperatureC ||
      std::fabs(record.scaled - 22.37) > 0.0001) {
    ESP_LOGE(TAG, "Portable C++ record decoder self-test failed");
    abort();
  }
  ESP_LOGI(TAG, "Portable C++ DIF/VIF self-test PASS: %.2f C", record.scaled);
  ESP_LOGI(TAG, "Free heap: %lu", static_cast<unsigned long>(esp_get_free_heap_size()));
  ESP_LOGI(TAG, "Largest free block: %lu",
           static_cast<unsigned long>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
}
