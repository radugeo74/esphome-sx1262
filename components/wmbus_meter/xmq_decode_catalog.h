#pragma once
// Generated-XMQ decode catalog. Driver metadata is centralized here;
// dispatch and meter processing do not need driver-specific table branches.
#include <cstddef>
#include <cstring>
#include "frame_bridge.h"
#include "xmq_generated.h"

namespace xmq_portable {
enum class PayloadPolicy : uint8_t { Application, PreferManufacturer0f };
struct DecodeDescriptor {
  const char *name;
  uint8_t ci;
  PayloadPolicy payload_policy;
  DriverTables tables;
};

inline PayloadStatus descriptor_payload(const DecodeDescriptor &descriptor,
    const uint8_t *frame, size_t length, size_t header_size,
    size_t suffix_size, int mfct_0f_index, PayloadSpan &out) {
  if (descriptor.payload_policy == PayloadPolicy::PreferManufacturer0f &&
      mfct_0f_index >= 0)
    return manufacturer_payload(frame, length, header_size, suffix_size,
                                mfct_0f_index, out);
  return application_payload(frame, length, header_size, suffix_size, out);
}

inline DriverStatus decode_descriptor_frame(const DecodeDescriptor &descriptor,
    const uint8_t *frame, size_t length, size_t header_size,
    size_t suffix_size, int mfct_0f_index,
    Capture *captures, size_t capture_capacity, DecodedPayload &output) {
  PayloadSpan payload{};
  if (descriptor_payload(descriptor, frame, length, header_size,
                         suffix_size, mfct_0f_index, payload) != PayloadStatus::Ok) {
    output.value_count = 0;
    output.date_count = 0;
    return DriverStatus::BadArguments;
  }
  return decode_driver_payload(descriptor.tables, payload.data, payload.length,
                               captures, capture_capacity, output);
}

inline const DecodeDescriptor *find_decode_descriptor(const char *name, uint8_t ci) {
  static const DecodeDescriptor catalog[] = {
    {"fhkvdataiii", 0xA0, PayloadPolicy::PreferManufacturer0f, {
      generated_ixml::fhkvdataiii::rules, generated_ixml::fhkvdataiii::count,
      generated_ixml::fhkvdataiii::capture_keys,
      sizeof(generated_ixml::fhkvdataiii::capture_keys) / sizeof(CaptureKey),
      generated_ixml::fhkvdataiii::field_keys,
      sizeof(generated_ixml::fhkvdataiii::field_keys) / sizeof(FieldKey),
      generated_ixml::fhkvdataiii::calculations,
      generated_ixml::fhkvdataiii::calculation_count,
      generated_ixml::fhkvdataiii::date_calculations,
      generated_ixml::fhkvdataiii::date_calculation_count}},
    {"mkradio4", 0xA2, PayloadPolicy::Application, {
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
