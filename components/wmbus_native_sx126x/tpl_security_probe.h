#pragma once
// Read-only, fail-closed classification of TPL encryption from an already
// DLL-CRC-verified, CRC-stripped wM-Bus frame. Never decrypts payloads.
// Manufacturer-specific CI fields and unknown transport layouts stay unknown.
#include <cstddef>
#include <cstdint>
#include "pure_frame_header.h"

namespace wmbus_pure {
namespace tpl_security {
enum class State : uint8_t { Unknown, NotEncrypted, Encrypted };
struct Result {
  State state{State::Unknown};
  uint8_t mode{0xFF};  // 0..15 when a supported configuration word was read
  bool has_config_word{false};
};
inline const char *label(State state) {
  switch (state) {
    case State::NotEncrypted: return "no";
    case State::Encrypted: return "yes";
    default: return "unknown";
  }
}
// 0x7A: CI + access(1) + status(1) + configuration(2).
// 0x72: CI + identification(4) + manufacturer(2) + version(1)
//       + medium(1) + access(1) + status(1) + configuration(2).
// Configuration word is little endian, security mode is bits 8..11.
inline Result inspect(const uint8_t *frame, size_t size) {
  Header header{};
  if (parse_header(frame, size, header) != HeaderStatus::Ok)
    return {};
  size_t config_offset = 0;
  if (header.ci == 0x7A) config_offset = 13;
  else if (header.ci == 0x72) config_offset = 21;
  else return {};  // Includes manufacturer-specific CI A0/A2.
  if (size < config_offset + 2) return {};
  const uint16_t cw = static_cast<uint16_t>(frame[config_offset]) |
                      (static_cast<uint16_t>(frame[config_offset + 1]) << 8);
  const uint8_t mode = static_cast<uint8_t>((cw >> 8) & 0x0F);
  Result result{};
  result.mode = mode;
  result.has_config_word = true;
  if (mode == 0) result.state = State::NotEncrypted;
  else result.state = State::Encrypted;
  return result;
}
}  // namespace tpl_security
}  // namespace wmbus_pure
