#pragma once
// HOST/PROBE-ONLY staging contract for the existing native sensor binding
// names. There are deliberately NO ESPHome headers or publish_state calls.
// All slots are constructed after a successful configured/authenticated decode;
// this does not authorize RF admission or live driver registration.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "kamwater_configured_fields.h"

namespace xmq_portable { namespace kamwater { namespace sensor_mapping {
enum class Status : uint8_t {
  Ok, InvalidReading, Capacity
};
struct Number {
  const char *name{nullptr};
  double value{0};
};
struct Text {
  const char *name{nullptr};
  const char *value{nullptr};
};
// Names follow existing xmq_sensors / xmq_text_sensors field bindings.
// Opaque acoustic noise is intentionally NOT mapped: current native
// xmq_text_sensors YAML permits status but no acoustic diagnostic.
// The authenticated CCM counter is transport metadata, never a sensor.
struct Plan {
  Number numbers[2]{};
  Text texts[1]{};
  size_t number_count{0};
  size_t text_count{0};
};
inline Status stage(const configured_fields::Reading &reading, Plan &out) {
  out={};
  if (!reading.has_total || !reading.has_target ||
      !std::isfinite(reading.total_m3) ||
      !std::isfinite(reading.target_m3) ||
      reading.total_m3<0 || reading.target_m3<0 ||
      reading.status.empty())
    return Status::InvalidReading;
  Plan candidate{};
  candidate.numbers[0]={"total_m3",reading.total_m3};
  candidate.numbers[1]={"target_m3",reading.target_m3};
  candidate.number_count=2;
  // Pointer remains valid only as long as the caller's Reading is alive.
  // This plan MUST be consumed synchronously; never retain it.
  candidate.texts[0]={"status",reading.status.c_str()};
  candidate.text_count=1;
  out=candidate;
  return Status::Ok;
}
}}} // namespace xmq_portable::kamwater::sensor_mapping
