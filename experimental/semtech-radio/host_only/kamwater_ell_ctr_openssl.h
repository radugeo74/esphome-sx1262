#pragma once
// HOST ONLY: ELL II AES-CTR experiment using OpenSSL AES-128 ECB blocks.
// Mirrors wmbusmeters decrypt_ELL_AES_CTR blockwise keystream and
// big-endian 128-bit counter increments. Never used by ESPHome firmware.
// CRC is non-cryptographic error detection, NOT message authentication.
#include <cstddef>
#include <cstdint>
#include <vector>
#include <openssl/evp.h>
#include "kamwater_ell_ctr_iv.h"
#include "../../../components/wmbus_native_sx126x/kamwater_compact_signature.h"
namespace xmq_portable { namespace kamwater { namespace ell_ctr {
enum class DecryptStatus : uint8_t {
  Ok, Invalid, Unsupported, CryptoFailure, BadCrc, UnsupportedTpl
};
// increment_counter() is shared with the native ESP-IDF keyless IV layer.
inline bool aes_ecb_block(const uint8_t key[16],const uint8_t input[16],
                          uint8_t output[16]) {
  EVP_CIPHER_CTX *ctx=EVP_CIPHER_CTX_new();
  if(!ctx) return false;
  int n=0;
  int tail=0;
  bool success=EVP_EncryptInit_ex(ctx,EVP_aes_128_ecb(),nullptr,key,nullptr)==1;
  if(success) success=EVP_CIPHER_CTX_set_padding(ctx,0)==1;
  if(success) success=EVP_EncryptUpdate(ctx,output,&n,input,16)==1&&n==16;
  if(success) success=EVP_EncryptFinal_ex(ctx,output+16,&tail)==1&&tail==0;
  EVP_CIPHER_CTX_free(ctx);
  return success;
}
inline DecryptStatus decrypt_ell_ii(
    const uint8_t manufacturer_le[2],const uint8_t address[6],
    const uint8_t key[16],const uint8_t *ell,size_t length,
    std::vector<uint8_t> &tpl) {
  tpl.clear();
  if(!manufacturer_le || !address || !key || !ell || length<12)
    return DecryptStatus::Invalid;
  Iv iv{};
  if(derive_iv(manufacturer_le,address,ell,length,iv)!=Status::Ok)
    return DecryptStatus::Unsupported;
  // First encrypted octet follows the four session bytes at offset 7.
  const size_t size=length-7;
  if(size<3 || size>245) return DecryptStatus::Invalid;
  std::vector<uint8_t> clear(size);
  for(size_t offset=0;offset<size;offset+=16) {
    uint8_t keystream[16]{};
    if(!aes_ecb_block(key,iv.bytes,keystream))
      return DecryptStatus::CryptoFailure;
    const size_t n=size-offset<16?size-offset:16;
    for(size_t j=0;j<n;++j) clear[offset+j]=ell[7+offset+j]^keystream[j];
    increment_counter(iv.bytes);
  }
  const uint16_t expected=static_cast<uint16_t>(clear[0]) |
                           (static_cast<uint16_t>(clear[1])<<8);
  const uint16_t actual=signature::crc16_en13757(clear.data()+2,size-2);
  if(expected!=actual) return DecryptStatus::BadCrc;
  if(clear[2]!=0x78 && clear[2]!=0x79)
    return DecryptStatus::UnsupportedTpl;
  tpl.assign(clear.begin()+2,clear.end());
  return DecryptStatus::Ok;
}
}}} // namespace xmq_portable::kamwater::ell_ctr
