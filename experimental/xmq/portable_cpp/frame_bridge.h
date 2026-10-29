#pragma once
// Borrow the application-data span from an already parsed/decrypted telegram.
// No fixed wM-Bus header offset or allocation. The caller retains ownership.
#include <cstddef>
#include <cstdint>
#include "driver_runtime.h"

namespace xmq_portable {
struct PayloadSpan { const uint8_t *data; size_t length; };
enum class PayloadStatus : uint8_t { Ok, InvalidBounds, EmptyPayload };
inline PayloadStatus application_payload(const uint8_t *frame, size_t size,
                                         size_t header_size, size_t suffix_size,
                                         PayloadSpan &out) {
  out = {nullptr, 0};
  if (!frame || header_size > size || suffix_size > size - header_size)
    return PayloadStatus::InvalidBounds;
  const size_t length = size - header_size - suffix_size;
  if (length == 0) return PayloadStatus::EmptyPayload;
  out = {frame + header_size, length};
  return PayloadStatus::Ok;
}
// The existing Telegram parser reports mfct_0f_index relative to the
// application-data start. XMQ grammars beginning with 0x0F need that marker
// included. A missing marker is not a license to guess an offset.
inline PayloadStatus manufacturer_payload(const uint8_t *frame, size_t size,
    size_t header_size, size_t suffix_size, int mfct_0f_index,
    PayloadSpan &out) {
  out = {nullptr, 0};
  PayloadSpan application{};
  const auto status = application_payload(frame, size, header_size,
                                          suffix_size, application);
  if (status != PayloadStatus::Ok) return status;
  if (mfct_0f_index < 0 ||
      static_cast<size_t>(mfct_0f_index) >= application.length)
    return PayloadStatus::InvalidBounds;
  const size_t offset = static_cast<size_t>(mfct_0f_index);
  if (application.data[offset] != 0x0F)
    return PayloadStatus::InvalidBounds;
  out = {application.data + offset, application.length - offset};
  return PayloadStatus::Ok;
}
inline DriverStatus decode_manufacturer_frame(const DriverTables &tables,
    const uint8_t *frame, size_t frame_length, size_t header_size,
    size_t suffix_size, int mfct_0f_index,
    Capture *captures, size_t capture_capacity, DecodedPayload &output) {
  PayloadSpan payload{};
  if (manufacturer_payload(frame, frame_length, header_size,
                           suffix_size, mfct_0f_index, payload) != PayloadStatus::Ok) {
    output.value_count = 0;
    output.date_count = 0;
    return DriverStatus::BadArguments;
  }
  return decode_driver_payload(tables, payload.data, payload.length,
                               captures, capture_capacity, output);
}
inline DriverStatus decode_parsed_frame(const DriverTables &tables,
    const uint8_t *frame, size_t frame_length,
    size_t header_size, size_t suffix_size,
    Capture *captures, size_t capture_capacity, DecodedPayload &output) {
  PayloadSpan payload{};
  if (application_payload(frame, frame_length, header_size,
                          suffix_size, payload) != PayloadStatus::Ok) {
    output.value_count = 0;
    output.date_count = 0;
    return DriverStatus::BadArguments;
  }
  return decode_driver_payload(tables, payload.data, payload.length,
                               captures, capture_capacity, output);
}
} // namespace xmq_portable
