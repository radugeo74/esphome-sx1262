#pragma once
// Inspect only the TPL configuration word. Never decrypt or mark data as
// plaintext based solely on this classification.
#include <cstdint>
#include "pure_transport.h"
namespace wmbus_pure {
enum class SecurityKind : uint8_t {
  NoSecurity, AesCbcIvMode5, AesCbcNoIvMode7, Unsupported, NotApplicable
};
struct SecurityInfo {
  SecurityKind kind{SecurityKind::NotApplicable};
  uint8_t mode{0};
  uint8_t encrypted_blocks{0};
  bool extended_configuration_required{false};
};
inline SecurityInfo inspect_tpl_security(const TransportEnvelope &envelope) {
  SecurityInfo info{};
  if (envelope.kind != TransportKind::Short &&
      envelope.kind != TransportKind::Long) return info;
  info.mode = static_cast<uint8_t>((envelope.configuration >> 8) & 0x1Fu);
  info.encrypted_blocks = static_cast<uint8_t>((envelope.configuration >> 4) & 0x0Fu);
  switch (info.mode) {
    case 0: info.kind = SecurityKind::NoSecurity; break;
    case 5: info.kind = SecurityKind::AesCbcIvMode5; break;
    case 7:
      info.kind = SecurityKind::AesCbcNoIvMode7;
      info.extended_configuration_required = true;
      break;
    default: info.kind = SecurityKind::Unsupported; break;
  }
  return info;
}
}  // namespace wmbus_pure
