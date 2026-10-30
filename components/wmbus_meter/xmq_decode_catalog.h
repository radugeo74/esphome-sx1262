#pragma once
// Generated-XMQ decode catalog. Driver metadata is centralized here;
// dispatch and meter processing do not need driver-specific table branches.
#include <cstddef>
#include <cstring>
#include "driver_runtime.h"
#include "xmq_generated.h"

namespace xmq_portable {
struct DecodeDescriptor {
  const char *name;
  uint8_t ci;
  DriverTables tables;
};

inline const DecodeDescriptor *find_decode_descriptor(const char *name, uint8_t ci) {
  static const DecodeDescriptor catalog[] = {
    {"fhkvdataiii", 0xA0, {
      generated_ixml::fhkvdataiii::rules, generated_ixml::fhkvdataiii::count,
      generated_ixml::fhkvdataiii::capture_keys,
      sizeof(generated_ixml::fhkvdataiii::capture_keys) / sizeof(CaptureKey),
      generated_ixml::fhkvdataiii::field_keys,
      sizeof(generated_ixml::fhkvdataiii::field_keys) / sizeof(FieldKey),
      generated_ixml::fhkvdataiii::calculations,
      generated_ixml::fhkvdataiii::calculation_count,
      generated_ixml::fhkvdataiii::date_calculations,
      generated_ixml::fhkvdataiii::date_calculation_count}},
    {"mkradio4", 0xA2, {
      generated_ixml::mkradio4::rules, generated_ixml::mkradio4::count,
      generated_ixml::mkradio4::capture_keys,
      sizeof(generated_ixml::mkradio4::capture_keys) / sizeof(CaptureKey),
      generated_ixml::mkradio4::field_keys,
      sizeof(generated_ixml::mkradio4::field_keys) / sizeof(FieldKey),
      generated_ixml::mkradio4::calculations,
      generated_ixml::mkradio4::calculation_count,
      nullptr, 0}}
  };
  if (!name) return nullptr;
  for (const auto &descriptor : catalog)
    if (ci == descriptor.ci && std::strcmp(name, descriptor.name) == 0)
      return &descriptor;
  return nullptr;
}
}  // namespace xmq_portable
