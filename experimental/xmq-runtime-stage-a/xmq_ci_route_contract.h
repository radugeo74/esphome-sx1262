#pragma once
// Host-only source-provenance routing contract for verified XMQ inputs.
// A manufacturer-specific CI (A0/A2) is NOT a standard TPL5 header.
// This does not decode radio, decrypt AES, or publish any HA sensor.
#include "xmq_telegram_capture.h"
#include <cstdint>

namespace xmq_runtime {

enum class XmqTelegramRoute : std::uint8_t {
  Reject,
  LegacyVerifiedProprietary,
  LegacyVerifiedStandard,
  DirectTpl5StandardCandidate
};

// A0/A2 must come from the successfully decoded legacy Telegram payload.
// Direct-mode TPL5 is restricted to standard CI72/7A and still requires
// independent validation outside this classifier.
inline XmqTelegramRoute classify_xmq_telegram_route(
    std::uint8_t ci, ApplicationEncoding encoding,
    bool legacy_decrypt_verified, bool dll_crc_verified,
    bool direct_tpl5_candidate) noexcept {
  if (!dll_crc_verified || !encoding_matches_ci(encoding, ci))
    return XmqTelegramRoute::Reject;
  if (ci == 0xA0 || ci == 0xA2)
    return legacy_decrypt_verified && !direct_tpl5_candidate
               ? XmqTelegramRoute::LegacyVerifiedProprietary
               : XmqTelegramRoute::Reject;
  if (ci == 0x72 || ci == 0x7A) {
    if (direct_tpl5_candidate && !legacy_decrypt_verified)
      return XmqTelegramRoute::DirectTpl5StandardCandidate;
    if (legacy_decrypt_verified && !direct_tpl5_candidate)
      return XmqTelegramRoute::LegacyVerifiedStandard;
  }
  return XmqTelegramRoute::Reject;
}
}  // namespace xmq_runtime
