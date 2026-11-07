#pragma once
// Native Kamwater ELL-II AES-128-CTR using ESP-IDF mbedTLS AES-ECB.
// Same IV and big-endian counter progression as original wmbusmeters.
// The two-byte decrypted ELL CRC is error detection, NOT a MAC.
// Only the configured, CRC-verified receiver may call this; no publication.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>
#include "mbedtls/aes.h"
#include "kamwater_ell_ctr_iv.h"
#include "kamwater_compact_signature.h"

namespace xmq_portable { namespace kamwater { namespace ell_ctr_mbedtls {
enum class Status : uint8_t {
  Ok, Invalid, UnsupportedMode, CryptoFailure, BadCrc, UnsupportedTpl
};
inline Status decrypt_ell_ii(
    const uint8_t manufacturer_le[2],const uint8_t address[6],
    const uint8_t key[16],const uint8_t *ell,size_t length,
    std::vector<uint8_t> &out_tpl) {
  out_tpl.clear();
  if(!manufacturer_le || !address || !key || !ell || length<12)
    return Status::Invalid;
  ell_ctr::Iv iv{};
  if(ell_ctr::derive_iv(manufacturer_le,address,ell,length,iv)!=
     ell_ctr::Status::Ok)
    return Status::UnsupportedMode;
  const size_t encrypted_length=length-7;
  if(encrypted_length<3 || encrypted_length>245)
    return Status::Invalid;
  std::vector<uint8_t> clear(encrypted_length);
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  const int configured=mbedtls_aes_setkey_enc(&aes,key,128);
  int result=configured;
  if(result==0) {
    for(size_t pos=0;pos<encrypted_length;pos+=16) {
      uint8_t stream[16]{};
      result=mbedtls_aes_crypt_ecb(&aes,MBEDTLS_AES_ENCRYPT,
                                   iv.bytes,stream);
      if(result!=0) break;
      const size_t available=encrypted_length-pos;
      const size_t n=available<16?available:16;
      for(size_t j=0;j<n;++j)
        clear[pos+j]=ell[7+pos+j]^stream[j];
      std::fill(stream,stream+16,0);
      ell_ctr::increment_counter(iv.bytes);
    }
  }
  mbedtls_aes_free(&aes);
  std::fill(iv.bytes,iv.bytes+16,0);
  if(result!=0) {
    std::fill(clear.begin(),clear.end(),0);
    return Status::CryptoFailure;
  }
  const uint16_t observed=static_cast<uint16_t>(clear[0]) |
      (static_cast<uint16_t>(clear[1])<<8);
  const uint16_t expected=signature::crc16_en13757(
      clear.data()+2,clear.size()-2);
  if(observed!=expected) {
    std::fill(clear.begin(),clear.end(),0);
    return Status::BadCrc;
  }
  if(clear[2]!=0x78 && clear[2]!=0x79) {
    std::fill(clear.begin(),clear.end(),0);
    return Status::UnsupportedTpl;
  }
  out_tpl.assign(clear.begin()+2,clear.end());
  std::fill(clear.begin(),clear.end(),0);
  return Status::Ok;
}
}}} // namespace xmq_portable::kamwater::ell_ctr_mbedtls
