#include <stdio.h>
#include <stdbool.h>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_system.h"

extern bool pure_crypto_probe(void);

void app_main(void) {
    ESP_LOGI("xmq_probe", "Pure AES/mBedTLS self-check: %s", pure_crypto_probe() ? "PASS" : "FAIL");
    ESP_LOGI("xmq_probe", "Free heap: %lu", (unsigned long)esp_get_free_heap_size());
    ESP_LOGI("xmq_probe", "Largest free block: %lu",
             (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}
