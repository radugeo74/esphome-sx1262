#pragma once
// This is a BUILD-ONLY compatibility shim for including the real
// wmbus_common::Telegram declaration in the isolated ESP-IDF smoke binary.
// Full ESPHome builds use esphome/core/log.h from ESPHome itself instead.
#include "esp_log.h"
