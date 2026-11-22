#pragma once
// Stage 4 HOST-ONLY: extract a candidate clear application directly from a
// radio-verified T1 DLL frame, BEFORE invoking legacy handleTelegram().
// This is NOT a live radio hook, security/decryption implementation, or
// authorization to write HA sensors. Do not put it in deployed components.
#include "xmq_telegram_capture.h"
#include <cstdint>
#include <vector>

namespace xmq_runtime {

enum class CandidateLinkMode : uint8_t { T1, C1, Unknown };

// Radio CRC and clear-application attestations are explicit trusted inputs;
// this adaptor cannot itself verify physical-layer CRC or security mode.
// The proprietary original format below is limited to clear TCH MK radio4
// fixtures. No encrypted C1 or unknown transports are admitted.
inline bool capture_t1_clear_mkradio4_candidate(
    const std::vector<uint8_t> &dll, CandidateLinkMode mode,
    bool radio_crc_verified, bool clear_application_verified,
    VerifiedApplication &destination) noexcept {
  destination = VerifiedApplication{};
  if (!radio_crc_verified || !clear_application_verified ||
      mode != CandidateLinkMode::T1 ||
      dll.size() < 16 || dll.size() > 256 ||
      static_cast<size_t>(dll.front()) + 1 != dll.size())
    return false;
  // L, C=SND_NR, M=TCH, ID(4), version, media, proprietary CI=A2.
  if (dll[1] != 0x44 || dll[2] != 0x68 || dll[3] != 0x50 ||
      (dll[8] != 0x95 && dll[8] != 0x70) ||
      (dll[9] != 0x62 && dll[9] != 0x72) || dll[10] != 0xA2)
    return false;
  // Restrict Stage 4 to the observed unencrypted MK4 application envelope.
  // The marker is format evidence, NOT cryptographic proof of cleartext.
  // Caller MUST independently prove clear application security status;
  // a frame with unknown security is rejected by the required boolean.
  if (dll[11] != 0x06 || !((dll[12] == 0x3e && dll[13] == 0x35) ||
                            (dll[12] == 0x70 && dll[13] == 0x29)))
    return false;
  try {
    VerifiedApplication out;
    out.inner_mvt = {"TCH", dll[8], dll[9]};
    out.encoding = ApplicationEncoding::OriginalIxml;
    out.payload.assign(dll.begin() + 11, dll.end());
    destination = std::move(out);
    return true;
  } catch (...) {
    destination = VerifiedApplication{};
    return false;
  }
}
} // namespace xmq_runtime
