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

// TPL security mode is encoded in bits 8..12 of the config word.
// Mode 5 uses AES-CBC with IV; mode 7 needs its own key derivation/MAC
// checks before decrypting. Flags below are evidence from a FUTURE audited
// decryptor, not a substitute for performing those cryptographic operations.
enum class PrimaryTplSecurity : std::uint8_t {
  Unknown = 255, None = 0, AesCbcIv = 5, AesCbcNoIv = 7
};
struct PrimaryDecryptEvidence {
  PrimaryTplSecurity mode{PrimaryTplSecurity::Unknown};
  bool decrypt_succeeded{false};
  bool cbc_check_bytes_valid{false};
  bool mac_authenticated{false};
};
inline PrimaryTplSecurity primary_security_from_tpl_cfg(
    std::uint16_t tpl_cfg) noexcept {
  switch ((tpl_cfg >> 8) & 0x1fU) {
    case 0: return PrimaryTplSecurity::None;
    case 5: return PrimaryTplSecurity::AesCbcIv;
    case 7: return PrimaryTplSecurity::AesCbcNoIv;
    default: return PrimaryTplSecurity::Unknown;
  }
}
inline bool primary_security_evidence_valid(
    const PrimaryDecryptEvidence &proof) noexcept {
  switch (proof.mode) {
    case PrimaryTplSecurity::None:
      return !proof.decrypt_succeeded && !proof.cbc_check_bytes_valid &&
             !proof.mac_authenticated;
    case PrimaryTplSecurity::AesCbcIv:
      return proof.decrypt_succeeded && proof.cbc_check_bytes_valid;
    case PrimaryTplSecurity::AesCbcNoIv:
      return proof.decrypt_succeeded && proof.cbc_check_bytes_valid &&
             proof.mac_authenticated;
    default:
      return false;
  }
}

struct PrimaryRadioApplication {
  bool dll_crc_verified{false};
  PrimaryLinkMode link_mode{PrimaryLinkMode::Unknown};
  PrimaryPayloadTrust payload_trust{PrimaryPayloadTrust::Unknown};
  PrimaryDecryptEvidence security{};
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
      !primary_security_evidence_valid(frame.security) ||
      frame.application.empty() || frame.application.size() > 1024)
    return false;
  return decode_primary_candidate(
      registry, frame.inner_mvt, frame.encoding, frame.application,
      expected_driver, required_numeric, required_dates, destination);
}
}  // namespace xmq_runtime
