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
// Parse only an immediately following, unencrypted, explicit TPL CI.
// No generic scan: unknown NWL/AFL, encrypted ELL, and unverified ELL
// payloads must not be mistaken for a verified meter address.
enum class NestedTplStatus : uint8_t {
  NotVerified, NoTpl, TooShort, InvalidManufacturer, ShortHeader, LongHeader
};
struct NestedTplResult {
  NestedTplStatus status{NestedTplStatus::NotVerified};
  Address address{};
  bool has_address{false};
  bool has_config_word{false};
  uint8_t tpl_security_mode{0xFF}; // advisory; never authorizes decoding
  size_t application_offset{0};
};
inline NestedTplResult inspect_nested_tpl(const uint8_t *frame,size_t size,
                                          const Result &identity) {
  NestedTplResult out{};
  if (!frame || identity.status!=Status::Ok ||
      inspect_ell_payload(frame,size,identity).status!=EllIntegrity::Valid)
    return out;
  const size_t at=identity.application_offset;
  if (at>=size) {out.status=NestedTplStatus::NoTpl;return out;}
  if (frame[at]==0x7A) {
    if (size-at<5) {out.status=NestedTplStatus::TooShort;return out;}
    const uint16_t cw=static_cast<uint16_t>(frame[at+3]) |
                      (static_cast<uint16_t>(frame[at+4])<<8);
    out.has_config_word=true;
    out.tpl_security_mode=static_cast<uint8_t>((cw>>8)&0x0F);
    out.status=NestedTplStatus::ShortHeader;
    out.application_offset=at+5;
    return out;
  }
  if (frame[at]==0x72) {
    if (size-at<13) {out.status=NestedTplStatus::TooShort;return out;}
    copy_address(out.address,frame+at+1,false);
    if (!valid_manufacturer(out.address.manufacturer)) {
      out.status=NestedTplStatus::InvalidManufacturer;return out;
    }
    const uint16_t cw=static_cast<uint16_t>(frame[at+11]) |
                      (static_cast<uint16_t>(frame[at+12])<<8);
    out.has_config_word=true;
    out.tpl_security_mode=static_cast<uint8_t>((cw>>8)&0x0F);
    out.status=NestedTplStatus::LongHeader;
    out.has_address=true;
    out.application_offset=at+13;
    return out;
  }
  out.status=NestedTplStatus::NoTpl; // Could be NWL/AFL, never guess.
  return out;
}
}} // namespace wmbus_pure::identity
