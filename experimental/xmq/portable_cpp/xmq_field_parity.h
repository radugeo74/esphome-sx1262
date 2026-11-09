#pragma once
// Allocation-free numeric field matching between independently decoded XMQ
// values and the authoritative legacy meter. Never publishes sensor values.
#include <cmath>
#include <cstddef>
#include <cstring>
#include "driver_runtime.h"

namespace xmq_portable {
struct ParityField {
  const char *xmq_name;
  const char *legacy_field;
  double tolerance;
};
enum class FieldParity : uint8_t { Equal, Different, MissingXmq, MissingLegacy };
inline FieldParity compare_numeric_field(const DecodedPayload &decoded,
    const ParityField &field, double legacy_value, bool legacy_valid,
    double &xmq_value) {
  if (!legacy_valid || !std::isfinite(legacy_value))
    return FieldParity::MissingLegacy;
  for (size_t i = 0; i < decoded.value_count; ++i) {
    if (decoded.values[i].name &&
        std::strcmp(decoded.values[i].name, field.xmq_name) == 0) {
      xmq_value = decoded.values[i].value;
      if (!std::isfinite(xmq_value)) return FieldParity::Different;
      return std::fabs(xmq_value - legacy_value) <= field.tolerance
          ? FieldParity::Equal : FieldParity::Different;
    }
  }
  return FieldParity::MissingXmq;
}
inline const ParityField *parity_fields(const char *driver, size_t &count) {
  static const ParityField heat[] = {
    {"current_hca", "current_hca", 0.000001},
    {"previous_hca", "previous_hca", 0.000001},
    {"temp_room_c", "temp_room_c", 0.000001},
    {"temp_radiator_c", "temp_radiator_c", 0.000001}
  };
  static const ParityField water[] = {
    {"target", "target_m3", 0.000001},
    {"total", "total_m3", 0.000001}
  };
  count = 0;
  if (!driver) return nullptr;
  if (std::strcmp(driver, "fhkvdataiii") == 0) {
    count = sizeof(heat)/sizeof(heat[0]); return heat;
  }
  if (std::strcmp(driver, "mkradio4") == 0) {
    count = sizeof(water)/sizeof(water[0]); return water;
  }
  return nullptr;
}
} // namespace xmq_portable
