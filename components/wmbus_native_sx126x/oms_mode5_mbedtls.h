#pragma once
// Shared native OMS short-TPL 0x7A AES-128-CBC mode-5 decryptor.
// Input MUST already be DLL-CRC-verified and CRC-stripped; this routine
// independently checks its frame layout, TPL security and block count.
// CBC offers NO authentication: 2F2F/0F are structural plausibility,
// not a MAC. No radio, YAML, legacy driver, or HA publication hook here.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>
#include "mbedtls/aes.h"
#include "pure_frame_header.h"

namespace xmq_portable { namespace oms_mode5 {
enum class Status : uint8_t {
  Ok, InvalidFrame, UnsupportedCi, UnsupportedMode,
  UnsupportedBlocks, MissingKey, CryptoFailure, BadMarker
};
struct Output {
  std::vector<uint8_t> clear_application{};
  uint8_t tpl_status{0};
};
inline Status decrypt_short_tpl_with_prefix(const uint8_t *frame,size_t length,
                                const uint8_t key[16],Output &out,
                                bool require_0f) {
  out=Output{};
  wmbus_pure::Header header{};
  if(wmbus_pure::parse_header(frame,length,header)!=
         wmbus_pure::HeaderStatus::Ok)
    return Status::InvalidFrame;
  if(header.ci!=0x7A) return Status::UnsupportedCi;
  if(length<15) return Status::InvalidFrame;
  const uint16_t config=static_cast<uint16_t>(frame[13]) |
      (static_cast<uint16_t>(frame[14])<<8);
  if(((config>>8)&31u)!=5u) return Status::UnsupportedMode;
  const size_t blocks=static_cast<size_t>((config>>4)&15u);
  // Mode-5: encrypted block count from configuration. Reject malformed
  // alignment, partial blocks and trailing bytes, never decrypt a prefix.
  if(blocks==0 || blocks>15 || (config&15u)!=0 ||
     length!=15+blocks*16u)
    return Status::UnsupportedBlocks;
  if(!key) return Status::MissingKey;
  uint8_t iv[16]{};
  std::memcpy(iv,frame+2,8);
  std::memset(iv+8,frame[11],8);
  const size_t encrypted_bytes=blocks*16u;
  std::vector<uint8_t> candidate(encrypted_bytes);
  mbedtls_aes_context ctx;
  mbedtls_aes_init(&ctx);
  const int key_status=mbedtls_aes_setkey_dec(&ctx,key,128);
  const int result=(key_status==0)
      ? mbedtls_aes_crypt_cbc(&ctx,MBEDTLS_AES_DECRYPT,encrypted_bytes,
                             iv,frame+15,candidate.data())
      : key_status;
  mbedtls_aes_free(&ctx);
  std::memset(iv,0,sizeof(iv));
  if(result!=0) {
    std::fill(candidate.begin(),candidate.end(),0);
    return Status::CryptoFailure;
  }
  if(candidate.size()<3 || candidate[0]!=0x2F ||
     candidate[1]!=0x2F || (require_0f && candidate[2]!=0x0F)) {
    std::fill(candidate.begin(),candidate.end(),0);
    return Status::BadMarker;
  }
  // The caller must validate the complete application grammar before
  // allowing any field; do not silently accept partial decodes.
  out.clear_application.assign(candidate.begin()+2,candidate.end());
  out.tpl_status=frame[12];
  std::fill(candidate.begin(),candidate.end(),0);
  return Status::Ok;
}
// Preserve the existing Apator162 strict 2F2F0F policy.
inline Status decrypt_short_tpl(const uint8_t *frame,size_t length,
                                const uint8_t key[16],Output &out) {
  return decrypt_short_tpl_with_prefix(frame,length,key,out,true);
}
// Integra Topas clear records begin after 2F2F without an 0F marker.
// Use ONLY with a strict model-specific complete application decoder.
// CBC/2F2F has no cryptographic authentication.
inline Status decrypt_short_tpl_record_payload(const uint8_t *frame,size_t length,
                                const uint8_t key[16],Output &out) {
  return decrypt_short_tpl_with_prefix(frame,length,key,out,false);
}
// The long-TPL variant carries the actual meter address after CI=0x72;
// it may be behind a separate DLL radio converter. Per OMS Mode-5, IV:
// TPL M(2 LE), TPL ID(4 LE), TPL V/T, TPL ACC repeated eight times.
// Only the record-decoder may interpret the decrypted payload.
inline Status decrypt_long_tpl(const uint8_t *frame,size_t length,
                               const uint8_t key[16],Output &out) {
  out=Output{};
  wmbus_pure::Header header{};
  if(wmbus_pure::parse_header(frame,length,header)!=
     wmbus_pure::HeaderStatus::Ok)
    return Status::InvalidFrame;
  if(header.ci!=0x72) return Status::UnsupportedCi;
  if(length<23) return Status::InvalidFrame;
  const uint16_t config=static_cast<uint16_t>(frame[21]) |
      (static_cast<uint16_t>(frame[22])<<8);
  if(((config>>8)&31u)!=5u) return Status::UnsupportedMode;
  const size_t blocks=static_cast<size_t>((config>>4)&15u);
  if(blocks==0 || blocks>15 || (config&15u)!=0 ||
     length!=23+blocks*16u)
    return Status::UnsupportedBlocks;
  if(!key) return Status::MissingKey;
  uint8_t iv[16]{};
  iv[0]=frame[15];iv[1]=frame[16];  // TPL manufacturer
  std::memcpy(iv+2,frame+11,4);   // TPL meter ID
  iv[6]=frame[17];iv[7]=frame[18]; // TPL version and medium
  std::memset(iv+8,frame[19],8);  // TPL access number
  const size_t encrypted_bytes=blocks*16u;
  std::vector<uint8_t> candidate(encrypted_bytes);
  mbedtls_aes_context ctx;
  mbedtls_aes_init(&ctx);
  const int key_status=mbedtls_aes_setkey_dec(&ctx,key,128);
  const int result=(key_status==0)
      ? mbedtls_aes_crypt_cbc(&ctx,MBEDTLS_AES_DECRYPT,encrypted_bytes,
                             iv,frame+23,candidate.data())
      : key_status;
  mbedtls_aes_free(&ctx);
  std::memset(iv,0,sizeof(iv));
  if(result!=0) {
    std::fill(candidate.begin(),candidate.end(),0);
    return Status::CryptoFailure;
  }
  if(candidate.size()<3 || candidate[0]!=0x2F ||
     candidate[1]!=0x2F) {
    std::fill(candidate.begin(),candidate.end(),0);
    return Status::BadMarker;
  }
  out.clear_application.assign(candidate.begin()+2,candidate.end());
  out.tpl_status=frame[20];
  std::fill(candidate.begin(),candidate.end(),0);
  return Status::Ok;
}
}} // namespace xmq_portable::oms_mode5
