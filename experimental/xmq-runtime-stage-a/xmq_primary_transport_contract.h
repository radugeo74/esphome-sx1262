#pragma once
// Host-only contract for a future direct wmbus_radio -> XMQ adapter.
// A CRC-verified T1/C1 DLL frame is NOT automatically decrypted. Refuse
// encrypted/unknown-cleartext traffic until an audited decryptor proves
// its exact authenticated application bytes and INNER meter metadata.
// No legacy meter, radio, or Home Assistant sensor side effects.
#include "xmq_primary_readiness.h"
#include <cstdint>
#include <string>
#include <vector>

namespace xmq_runtime {

enum class PrimaryLinkMode : std::uint8_t { Unknown, T1, C1 };
enum class PrimaryPayloadTrust : std::uint8_t {
  Unknown, Ciphertext, VerifiedCleartext
};

struct PrimaryRadioApplication {
  bool dll_crc_verified{false};
  PrimaryLinkMode link_mode{PrimaryLinkMode::Unknown};
  PrimaryPayloadTrust payload_trust{PrimaryPayloadTrust::Unknown};
  Mvt inner_mvt{};
  ApplicationEncoding encoding{ApplicationEncoding::StandardDv};
  std::vector<std::uint8_t> application;
};

// The caller, NOT this API, must establish the CRC/decrypt/inner-MVT
// properties. This gate rejects absent provenance; it does not invent it.
// For encrypted frames, a future AES integration must explicitly supply
// VerifiedCleartext after successful decryption and security checks.
inline bool decode_primary_radio_application(
    const DriverRegistry &registry, const PrimaryRadioApplication &frame,
    const std::string &expected_driver,
    const std::vector<std::string> &required_numeric,
    const std::vector<std::string> &required_dates,
    XmqFrameResult &destination) noexcept {
  destination = XmqFrameResult{};
  if (!frame.dll_crc_verified ||
      (frame.link_mode != PrimaryLinkMode::T1 &&
       frame.link_mode != PrimaryLinkMode::C1) ||
      frame.payload_trust != PrimaryPayloadTrust::VerifiedCleartext ||
      frame.application.empty() || frame.application.size() > 1024)
    return false;
  return decode_primary_candidate(
      registry, frame.inner_mvt, frame.encoding, frame.application,
      expected_driver, required_numeric, required_dates, destination);
}
}  // namespace xmq_runtime
