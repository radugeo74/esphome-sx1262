#pragma once
// Strict read-only ELL II (CI 8D) checksum/security inspection, following
// wmbusmeters src/wmbus.cc parseELL. Input begins at ELL CI and includes
// the TPL CI in the ELL CRC-covered payload. DLL CRC verification is an
// independent prerequisite handled by the caller. NO decryption or keys.
#include <cstddef>
#include <cstdint>
#include "kamwater_compact_signature.h"
namespace xmq_portable { namespace kamwater { namespace ell {
enum class Status : uint8_t {
  ClearAndCrcValid, Invalid, UnsupportedCi, Encrypted, ReservedSecurity,
  BadPayloadCrc, UnsupportedTpl
};
struct ClearPayload {
  const uint8_t *tpl{nullptr};
  size_t tpl_size{0};
  uint32_t session_number{0};
};
inline Status inspect_ell_ii(const uint8_t *data, size_t length,
                             ClearPayload &out) {
  out={};
  if(!data || length<11) return Status::Invalid;
  if(data[0]!=0x8D) return Status::UnsupportedCi;
  // ELL II: CI, CC, ACC, SN(4 LE), payload CRC(2 LE), TPL...
  const uint32_t sn=static_cast<uint32_t>(data[3]) |
      (static_cast<uint32_t>(data[4])<<8) |
      (static_cast<uint32_t>(data[5])<<16) |
      (static_cast<uint32_t>(data[6])<<24);
  const uint8_t sec=static_cast<uint8_t>((sn>>29)&7u);
  if(sec==1) return Status::Encrypted; // AES-CTR, no decrypt here
  if(sec!=0) return Status::ReservedSecurity;
  const size_t start=9;
  const uint16_t expected=static_cast<uint16_t>(data[7]) |
                           (static_cast<uint16_t>(data[8])<<8);
  const uint16_t actual=signature::crc16_en13757(data+start,length-start);
  if(expected!=actual) return Status::BadPayloadCrc;
  if(data[start]!=0x78 && data[start]!=0x79) return Status::UnsupportedTpl;
  out.tpl=data+start;
  out.tpl_size=length-start;
  out.session_number=sn;
  return Status::ClearAndCrcValid;
}
}}} // namespace xmq_portable::kamwater::ell
