#include <cmath>
#include <cstdint>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "record_decoder.h"
#include "generated_ixml.h"
#include "numeric_bridge.h"
#include <cstring>

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

  // Real IXML rule tables generated from unmodified upstream XMQ definitions.
  const uint8_t water[] = {0x06, 0x70, 0x29, 0x01, 0x00, 0x60, 0x17,
                           0x03, 0x00, 0x04, 0x00, 0x03, 0x00, 0x00, 0x00};
  Capture captured[8]{};
  Result parsed{captured, 8, 0, 0};
  const ParseStatus parsed_status = execute(
      generated_ixml::mkradio4::rules, generated_ixml::mkradio4::count,
      "decode", water, sizeof(water), parsed);
  if (parsed_status != ParseStatus::Ok || parsed.count != 2 ||
      parsed.consumed != sizeof(water) ||
      std::strcmp(parsed.captures[0].name, "DV_prev") != 0 ||
      parsed.captures[0].length != 2 ||
      parsed.captures[0].data[0] != 1 ||
      std::strcmp(parsed.captures[1].name, "DV_curr") != 0 ||
      parsed.captures[1].length != 2 ||
      parsed.captures[1].data[0] != 3) {
    ESP_LOGE(TAG, "Generated IXML C++ self-test failed");
    abort();
  }
  ESP_LOGI(TAG, "Generated mkradio4 IXML self-test PASS, captures=%u",
           static_cast<unsigned>(parsed.count));
  NumericValue numeric{};
  if (decode_mapped_water(parsed, generated_ixml::mkradio4::capture_keys, 2, generated_ixml::mkradio4::field_keys, 2, numeric) != NumericStatus::Ok ||
      std::fabs(numeric.target_m3 - 0.1) > 0.00001 ||
      std::fabs(numeric.total_m3 - 0.4) > 0.00001) {
    ESP_LOGE(TAG, "C++ IXML-to-numeric self-test failed");
    abort();
  }
  ESP_LOGI(TAG, "C++ water values: target=%.3f m3 total=%.3f m3",
           numeric.target_m3, numeric.total_m3);
  ESP_LOGI(TAG, "Free heap: %lu", static_cast<unsigned long>(esp_get_free_heap_size()));
  ESP_LOGI(TAG, "Largest free block: %lu",
           static_cast<unsigned long>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
}
