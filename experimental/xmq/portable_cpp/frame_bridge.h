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
