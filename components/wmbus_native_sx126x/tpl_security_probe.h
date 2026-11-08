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
  uint8_t mode{0xFF};  // OMS 5-bit security mode (0..31), if CF is present
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
// OMS CF is little endian: security mode M = bits 8..12 (five bits),
// encrypted block count N = bits 4..7. For mode 0 (no encryption),
// OMS Vol. 2 §7.2.4.2 requires BOTH M=0 and N=0.
// Other CF flags (B/A/S/C/R/H) do not by themselves imply encryption.
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
  const uint8_t mode = static_cast<uint8_t>((cw >> 8) & 0x1F);
  const uint8_t encrypted_blocks = static_cast<uint8_t>((cw >> 4) & 0x0F);
  Result result{};
  result.mode = mode;
  result.has_config_word = true;
  if (mode == 0 && encrypted_blocks == 0)
    result.state = State::NotEncrypted;
  else if (mode == 4 || mode == 5 || mode == 7 ||
           mode == 10 || mode == 13)
    result.state = State::Encrypted;
  // Mode 0 with N!=0 is inconsistent and MUST NOT be published as clear.
  // Other unsupported/reserved modes remain Unknown (fail closed).
  return result;
}
}  // namespace tpl_security
}  // namespace wmbus_pure
