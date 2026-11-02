#pragma once
// Single fail-closed bridge: native packet boundary -> EN13757 DLL CRC -> meter header.
// No handlers, legacy radio control, or sensor publishing.
#include "native_frame_boundary.h"
#include "pure_complete_frame.h"

namespace esphome {
namespace wmbus_native_sx126x {
namespace verified {
enum class Status : uint8_t { Ok, BadBoundary, BadCrcOrHeader };
struct Result {
  Status status{Status::BadBoundary};
  boundary::Status boundary_status{boundary::Status::InvalidInput};
  wmbus_pure::CompleteStatus verify_status{wmbus_pure::CompleteStatus::TooShort};
  wmbus_pure::CompleteFrame frame{};
};
inline Result process(boundary::Mode mode, const uint8_t *bytes, size_t size) {
  Result output{};
  auto framed = boundary::convert(mode, bytes, size);
  output.boundary_status = framed.status;
  if (framed.status != boundary::Status::Framed) return output;
  output.verify_status = wmbus_pure::verify_complete_frame(
      framed.decoded_dll_with_crc.data(), framed.decoded_dll_with_crc.size(),
      mode == boundary::Mode::C1B ? wmbus_pure::CrcFrameFormat::B :
                                    wmbus_pure::CrcFrameFormat::A,
      output.frame);
  if (output.verify_status != wmbus_pure::CompleteStatus::Ok) {
    output.frame = {};
    output.status = Status::BadCrcOrHeader;
    return output;
  }
  output.status = Status::Ok;
  return output;
}
} // namespace verified
} // namespace wmbus_native_sx126x
} // namespace esphome
