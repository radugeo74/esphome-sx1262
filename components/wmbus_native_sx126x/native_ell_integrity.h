#pragma once
// ELL payload-integrity probe for a DLL-CRC-verified, CRC-stripped frame.
// Does not decrypt, parse TPL, select drivers, or permit publication.
// ELL modes other than known plaintext (zero) remain unverified.
#include "native_identity_probe.h"
#include "pure_dll_crc.h"
namespace wmbus_pure { namespace identity {
enum class EllIntegrity : uint8_t {
  Unsupported, NoPayloadCrc, EncryptedOrUnknown, Truncated, Valid, BadCrc
};
struct EllIntegrityResult {
  EllIntegrity status{EllIntegrity::Unsupported};
  uint16_t expected{0};
  uint16_t calculated{0};
};
inline EllIntegrityResult inspect_ell_payload(
    const uint8_t *frame,size_t size,const Result &identity) {
  EllIntegrityResult result{};
  if (!frame || identity.status!=Status::Ok ||
      (identity.ci!=0x8C && identity.ci!=0x8D &&
       identity.ci!=0x8E && identity.ci!=0x8F))
    return result;
  if (!identity.has_ell_session) {
    result.status=EllIntegrity::NoPayloadCrc;return result;
  }
  if (identity.application_offset<2 || identity.application_offset>size) {
    result.status=EllIntegrity::Truncated;return result;
  }
  if (identity.ell_security_mode!=0) {
    result.status=EllIntegrity::EncryptedOrUnknown;return result;
  }
  const size_t crc_at=identity.application_offset-2;
  result.expected=static_cast<uint16_t>(frame[crc_at]) |
                  (static_cast<uint16_t>(frame[crc_at+1])<<8);
  result.calculated=crc16_en13757(frame+identity.application_offset,
                                  size-identity.application_offset);
  result.status=result.expected==result.calculated ?
                EllIntegrity::Valid : EllIntegrity::BadCrc;
  return result;
}
}} // namespace wmbus_pure::identity
