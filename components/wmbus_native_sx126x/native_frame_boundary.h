#pragma once
// Common diagnostic output for native SX126x FSK packets.
// No hardware assumptions about the T1 sync word. No meter dispatch.
#include <cstddef>
#include <cstdint>
#include <vector>
#include "c1_packet_framing.h"
#include "t1_3of6.h"
#include "t1_packet_framing.h"

namespace esphome {
namespace wmbus_native_sx126x {
namespace boundary {
enum class Mode : uint8_t { T1, C1A, C1B };
enum class Status : uint8_t { Framed, InvalidInput, Incomplete, InvalidSymbol, TooLong };
struct Result {
  Mode mode{Mode::T1};
  Status status{Status::InvalidInput};
  std::vector<uint8_t> decoded_dll_with_crc; // no radio sync; not validated
};
inline Result convert(Mode mode, const uint8_t *input, size_t size,
                      size_t t1_decoded_length = 0) {
  Result result;
  result.mode = mode;
  if (mode == Mode::T1) {
    if (t1_decoded_length == 0) {
      const auto framing = t1::frame_from_post_sync(input, size, result.decoded_dll_with_crc);
      switch (framing) {
        case t1::FrameStatus::Ok: result.status = Status::Framed; break;
        case t1::FrameStatus::Incomplete: result.status = Status::Incomplete; break;
        case t1::FrameStatus::BadSymbol: result.status = Status::InvalidSymbol; break;
        case t1::FrameStatus::TooLong: result.status = Status::TooLong; break;
        default: result.status = Status::InvalidInput; break;
      }
      return result;
    }
    const auto status = t1::decode(input, size, t1_decoded_length, result.decoded_dll_with_crc);
    switch (status) {
      case t1::DecodeStatus::Ok: result.status = Status::Framed; break;
      case t1::DecodeStatus::Truncated: result.status = Status::Incomplete; break;
      case t1::DecodeStatus::BadSymbol: result.status = Status::InvalidSymbol; break;
      case t1::DecodeStatus::Oversize: result.status = Status::TooLong; break;
      default: result.status = Status::InvalidInput; break;
    }
    return result;
  }
  std::vector<uint8_t> framed;
  const auto status = c1::frame_from_post_sync(input, size,
      mode == Mode::C1B ? 0x3D : 0xCD, framed);
  switch (status) {
    case c1::FrameStatus::Ok:
      // Strip only the 2-byte radio sync; leave DLL CRC bytes unmodified.
      result.decoded_dll_with_crc.assign(framed.begin() + 2, framed.end());
      result.status = Status::Framed;
      break;
    case c1::FrameStatus::Incomplete: result.status = Status::Incomplete; break;
    case c1::FrameStatus::TooLong: result.status = Status::TooLong; break;
    default: result.status = Status::InvalidInput; break;
  }
  return result;
}
} // namespace boundary
} // namespace wmbus_native_sx126x
} // namespace esphome
