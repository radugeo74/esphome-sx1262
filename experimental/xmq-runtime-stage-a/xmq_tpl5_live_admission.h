#pragma once
// HOST-ONLY admission contract before promoting a synthetic TPL5 decode
// into a candidate live XMQ application. Caller-supplied CRC status,
// IV/AES check bytes or matching MVT do NOT establish real provenance.
// CI 0x72/0x7A signals StandardDv, NOT OriginalIxml (proprietary).
#include "xmq_tpl5_identity_gate.h"
#include "xmq_primary_transport_contract.h"

namespace xmq_runtime {

inline bool tpl5_encoding_admissible(
    std::uint8_t ci, ApplicationEncoding encoding) noexcept {
  return (ci == 0x72 || ci == 0x7A) &&
         encoding == ApplicationEncoding::StandardDv;
}

inline bool admit_direct_tpl5_xmq_candidate(
    const ParsedTpl5 &parsed, const Tpl5IdentityCandidate &identity,
    std::uint8_t ci, ApplicationEncoding encoding,
    PrimaryPayloadTrust trust) noexcept {
  return parsed.valid && identity.accepted &&
         tpl5_encoding_admissible(ci, encoding) &&
         trust == PrimaryPayloadTrust::VerifiedCleartext;
}

// Direct TPL5 AES-CBC without an independent *live* provenance verifier
// must NOT claim VerifiedCleartext based only on CRC + 2F2F check bytes.
// Future supported sources must supply verifiable evidence externally;
// this conservative helper has no such source and always returns false.
inline bool tpl5_cbc_sentinel_proves_verified_cleartext() noexcept {
  return false;
}

}  // namespace xmq_runtime
