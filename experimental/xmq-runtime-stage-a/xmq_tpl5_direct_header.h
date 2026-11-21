#pragma once
// HOST-ONLY strict TPL5 header subset for CRC-verified DLL frames.
// Supports direct DLL->TPL CI=0x7A (short) or 0x72 (long), mode 5 ONLY.
// Refuses ELL/NWL/AFL, manufacturer-specific CI, mode 7 and ambiguous frames.
// CRC verification is external; this parser never asserts cryptographic
// authenticity and never reads meter keys, global state or sensors.
#include "xmq_tpl5_cbc.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace xmq_runtime {

struct ParsedTpl5 {
  bool valid{false};
  Tpl5HeaderFields fields{};
  std::uint16_t cfg{};
  std::uint8_t encrypted_blocks{};
  std::size_t payload_offset{};
  bool inner_tpl_address{false};
};

inline ParsedTpl5 parse_direct_tpl5_header(
    const std::vector<std::uint8_t> &frame,
    bool dll_crc_verified) noexcept {
  ParsedTpl5 result{};
  // DLL L, C, M[2], A[6], CI, plus TPL header.
  if (!dll_crc_verified || frame.size() < 16 || frame.size() > 255 ||
      static_cast<std::size_t>(frame[0]) + 1 != frame.size())
    return result;
  const std::uint8_t ci = frame[10];
  std::size_t cursor = 11;
  if (ci == 0x72) {
    // Long TPL: inner ID[4], manufacturer[2], version, type.
    if (frame.size() < cursor + 8 + 4 + 16) return result;
    result.inner_tpl_address = true;
    std::copy_n(frame.begin() + cursor, 4, result.fields.address_a.begin());
    cursor += 4;
    result.fields.mfct_le = {frame[cursor], frame[cursor + 1]};
    cursor += 2;
    result.fields.address_a[4] = frame[cursor++];
    result.fields.address_a[5] = frame[cursor++];
  } else if (ci == 0x7A) {
    // Short TPL: use DLL M/A only. Do not guess through ELL or AFL.
    if (frame.size() < cursor + 4 + 16) return result;
    result.fields.mfct_le = {frame[2], frame[3]};
    std::copy_n(frame.begin() + 4, 6, result.fields.address_a.begin());
  } else {
    return result;
  }
  // ACC, STS, TPL cfg little endian. Reject any truncated header.
  if (frame.size() < cursor + 4) return ParsedTpl5{};
  result.fields.access = frame[cursor++];
  ++cursor;  // STS, not part of mode-5 IV
  result.cfg = static_cast<std::uint16_t>(frame[cursor]) |
               (static_cast<std::uint16_t>(frame[cursor + 1]) << 8);
  cursor += 2;
  // Require mode 5, no unexpected extended security header in this subset.
  if (((result.cfg >> 8) & 0x1fU) != 5) return ParsedTpl5{};
  result.encrypted_blocks = static_cast<std::uint8_t>(
      (result.cfg >> 4) & 0x0f);
  const std::size_t remaining = frame.size() - cursor;
  const std::size_t encrypted_bytes =
      result.encrypted_blocks == 0 ? remaining
                                   : result.encrypted_blocks * 16U;
  if (encrypted_bytes < 16 || encrypted_bytes > 512 ||
      encrypted_bytes > remaining || encrypted_bytes % 16 != 0)
    return ParsedTpl5{};
  result.payload_offset = cursor;
  result.valid = true;
  return result;
}

// Caller must have verified DLL CRC before providing the trimmed frame.
// In a future radio adapter, the caller must also validate meter address,
// source identity, exact TPL configuration and security policies.
inline Tpl5CbcResult decrypt_direct_tpl5_frame(
    const std::vector<std::uint8_t> &frame,
    bool dll_crc_verified,
    const std::array<std::uint8_t, 16> &key) noexcept {
  const auto parsed = parse_direct_tpl5_header(frame, dll_crc_verified);
  if (!parsed.valid) return {};
  try {
    const std::vector<std::uint8_t> app(
        frame.begin() + parsed.payload_offset, frame.end());
    return decrypt_tpl5_application(
        app, key, parsed.fields, parsed.encrypted_blocks);
  } catch (...) {
    return {};
  }
}

}  // namespace xmq_runtime
