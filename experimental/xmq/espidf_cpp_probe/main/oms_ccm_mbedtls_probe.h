#pragma once
// ESP-IDF PROBE ONLY: OMS TPL security profile D, short 0x7A, mode 10.
// Mirrors the already-tested host-only OpenSSL golden subset, but uses
// mbedTLS CMAC/CCM from ESP-IDF. Not included by the live radio component.
// No partial decryption, no application output on invalid tag or input.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>
#include "mbedtls/cipher.h"
#include "mbedtls/cmac.h"
#include "mbedtls/ccm.h"

namespace xmq_portable { namespace oms_ccm_idf_probe {

enum class Status : uint8_t {
  Ok, Invalid, UnsupportedCi, UnsupportedMode, UnsupportedKdf,
  UnsupportedPartial, MissingKey, CryptoError, BadTag
};
struct Output {
  std::vector<uint8_t> clear_application{};
  uint8_t tpl_status{0};
  uint32_t counter{0};
};

inline bool derive_kenc(const uint8_t master_key[16],
                        const uint8_t counter_le[4],
                        const uint8_t meter_id_le[4],
                        uint8_t derived[16]) {
  if (!master_key || !counter_le || !meter_id_le || !derived) return false;
  const uint8_t cmac_input[16] = {
    0x00, counter_le[0], counter_le[1], counter_le[2], counter_le[3],
    meter_id_le[0], meter_id_le[1], meter_id_le[2], meter_id_le[3],
    0x07,0x07,0x07,0x07,0x07,0x07,0x07
  };
  const mbedtls_cipher_info_t *info =
      mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_AES_128_ECB);
  if (!info) return false;
  const int status=mbedtls_cipher_cmac(
      info,master_key,128,cmac_input,sizeof(cmac_input),derived);
  return status==0;
}

inline Status decode_short_tpl(const uint8_t dll_m[2],
                               const uint8_t dll_id[4],
                               uint8_t version,uint8_t medium,
                               const uint8_t master_key[16],
                               const uint8_t *tpl,size_t length,Output &out) {
  out={};
  if (!dll_m || !dll_id || !tpl || length<16 || length>256)
    return Status::Invalid;
  if (tpl[0]!=0x7A) return Status::UnsupportedCi;
  const uint16_t config=static_cast<uint16_t>(tpl[3]) |
      (static_cast<uint16_t>(tpl[4])<<8);
  if (((config>>8)&31u)!=10) return Status::UnsupportedMode;
  if ((config&0xFFu)!=0xFFu) return Status::UnsupportedPartial;
  const uint16_t cfe=static_cast<uint16_t>(tpl[5]) |
      (static_cast<uint16_t>(tpl[6])<<8);
  if (((cfe>>4)&3u)!=1) return Status::UnsupportedKdf;
  const size_t tag_size=4u+4u*((cfe>>8)&3u);
  const bool has_version=((cfe>>6)&1u)!=0;
  const size_t counter_offset=7u+(has_version?1u:0u);
  const size_t payload_offset=counter_offset+4u;
  if (length<=payload_offset+tag_size) return Status::Invalid;
  if (!master_key) return Status::MissingKey;

  const uint8_t *counter=tpl+counter_offset;
  uint8_t key[16]{};
  if (!derive_kenc(master_key,counter,dll_id,key)) return Status::CryptoError;
  // OMS nonce: manufacturer, identity, version, medium, 00, counter BE.
  const uint8_t nonce[13]={
    dll_m[0],dll_m[1],dll_id[0],dll_id[1],dll_id[2],dll_id[3],
    version,medium,0,counter[3],counter[2],counter[1],counter[0]
  };
  const size_t cipher_length=length-payload_offset-tag_size;
  std::vector<uint8_t> candidate(cipher_length);
  mbedtls_ccm_context ccm;
  mbedtls_ccm_init(&ccm);
  const int key_result=mbedtls_ccm_setkey(&ccm,MBEDTLS_CIPHER_ID_AES,key,128);
  const int auth_result=key_result==0
      ? mbedtls_ccm_auth_decrypt(&ccm,cipher_length,nonce,sizeof(nonce),
          tpl,counter_offset,tpl+payload_offset,candidate.data(),
          tpl+length-tag_size,tag_size)
      : key_result;
  mbedtls_ccm_free(&ccm);
  std::memset(key,0,sizeof(key));
  if (auth_result!=0) {
    if (!candidate.empty()) std::memset(candidate.data(),0,candidate.size());
    return key_result==0 ? Status::BadTag : Status::CryptoError;
  }
  out.clear_application.swap(candidate);
  out.tpl_status=tpl[2];
  out.counter=static_cast<uint32_t>(counter[0]) |
      (static_cast<uint32_t>(counter[1])<<8) |
      (static_cast<uint32_t>(counter[2])<<16) |
      (static_cast<uint32_t>(counter[3])<<24);
  return Status::Ok;
}
}} // namespace xmq_portable::oms_ccm_idf_probe
