#pragma once
// Generated-XMQ decode catalog. Driver metadata is centralized here;
// dispatch and meter processing do not need driver-specific table branches.
#include <cstddef>
#include <cstring>
#include "frame_bridge.h"
#include "xmq_generated.h"
#include "../wmbus_radio/pure_frame_header.h"
#include "../wmbus_radio/pure_meter_match.h"

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

// Explicit handoff of an already decrypted, validated Mode-5 application.
// The 2F 2F marker is checked by the existing AES decoder, but checked again
// here so an accidental direct call cannot skip the protocol prefix.
// This does NOT establish that arbitrary Mode-5 data matches an XMQ driver.
inline DriverStatus decode_mode5_plain_payload(
    const DecodeDescriptor &descriptor, const uint8_t *plaintext,
    size_t length, Capture *captures, size_t capture_capacity,
    DecodedPayload &output) {
  output.value_count = 0;
  output.date_count = 0;
  if (!plaintext || length <= 2 ||
      plaintext[0] != 0x2F || plaintext[1] != 0x2F)
    return DriverStatus::BadArguments;
  return decode_driver_payload(descriptor.tables, plaintext + 2,
                               length - 2, captures, capture_capacity, output);
}

enum class PureDecodeStatus : uint8_t {
  Ok, InvalidFrame, InvalidMeterId, UnsupportedTransport, UnknownDriver, DecodeFailure
};

// Input is already DLL-CRC-verified and CRC-stripped by the radio layer.
// Does not decrypt encrypted frames; callers must provide plain data.
// Never expose old decoded values after any failure.
inline PureDecodeStatus decode_configured_plain_frame(
    const uint8_t *frame, size_t length, const char *configured_id,
    const char *driver_name, Capture *captures, size_t capture_capacity,
    DecodedPayload &output) {
  output.value_count = 0;
  output.date_count = 0;
  wmbus_pure::Header header{};
  if (wmbus_pure::parse_header(frame, length, header) !=
      wmbus_pure::HeaderStatus::Ok)
    return PureDecodeStatus::InvalidFrame;
  if (wmbus_pure::match_meter_id(header, configured_id) !=
      wmbus_pure::MatchStatus::Match)
    return PureDecodeStatus::InvalidMeterId;
  // This path accepts only the two supported Techem manufacturer CIs.
  // A valid DLL header does NOT prove that a TPL payload is plaintext.
  // In particular CI=72/7A must be processed by a separate, security-aware
  // transport pipeline, never by the generated manufacturer decoders.
  if (header.ci != 0xA0 && header.ci != 0xA2)
    return PureDecodeStatus::UnsupportedTransport;
  const auto *descriptor = find_decode_descriptor(driver_name, header.ci);
  // The upstream Techem 0F HCA fixture uses CI=A2. Allow this precise
  // variant only for the configured heat driver, matching manufacturer,
  // medium, version and the manufacturer-specific 0F prefix.
  // Do not widen the catalog's generic A2 driver selection.
  if (!descriptor && driver_name &&
      std::strcmp(driver_name, "fhkvdataiii") == 0 &&
      header.manufacturer == 0x5068 && header.device_type == 0x80 &&
      header.version == 0x94 && header.ci == 0xA2 &&
      header.application_length > 0 && header.application[0] == 0x0F)
    descriptor = find_decode_descriptor("fhkvdataiii", 0xA0);
  if (!descriptor) return PureDecodeStatus::UnknownDriver;
  return decode_driver_payload(descriptor->tables, header.application,
                               header.application_length, captures,
                               capture_capacity, output) == DriverStatus::Ok
      ? PureDecodeStatus::Ok : PureDecodeStatus::DecodeFailure;
}
}  // namespace xmq_portable
