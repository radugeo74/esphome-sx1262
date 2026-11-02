#pragma once
// Strict, side-effect-free frame verification. The input is a decoded DLL
// frame (C1 preamble removed or T1 3-of-6 decoded) still containing CRC.
#include <cstddef>
#include <cstdint>
#include <vector>
#include "pure_dll_crc.h"
#include "pure_frame_header.h"

namespace wmbus_pure {
enum class CompleteStatus : uint8_t {
  Ok, TooShort, InvalidLength, BadCrc, UnsupportedHeader
};
struct CompleteFrame {
  Header header{};
  HeaderStatus header_status{HeaderStatus::TooShort};
  std::vector<uint8_t> bytes;
};
inline CompleteStatus verify_complete_frame(const uint8_t *frame, size_t size,
                                             CrcFrameFormat format,
                                             CompleteFrame &result) {
  // Fail closed: the caller must never receive stale data on an error.
  result = {};
  std::vector<uint8_t> verified;
  const CrcStatus crc = strip_dll_crc(frame, size, format, verified);
  if (crc == CrcStatus::TooShort) return CompleteStatus::TooShort;
  if (crc == CrcStatus::LengthMismatch) return CompleteStatus::InvalidLength;
  if (crc != CrcStatus::Ok) return CompleteStatus::BadCrc;
  Header parsed{};
  result.header_status = parse_header(verified.data(), verified.size(), parsed);
  if (result.header_status != HeaderStatus::Ok)
    return CompleteStatus::UnsupportedHeader;
  // Header.application points into the vector storage. Move the bytes first,
  // then reconstruct the pointer from the owned vector.
  result.bytes = std::move(verified);
  result.header_status = parse_header(result.bytes.data(), result.bytes.size(), result.header);
  return result.header_status == HeaderStatus::Ok ? CompleteStatus::Ok : CompleteStatus::UnsupportedHeader;
}
}  // namespace wmbus_pure
