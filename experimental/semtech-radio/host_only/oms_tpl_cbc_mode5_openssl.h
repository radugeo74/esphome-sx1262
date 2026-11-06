#pragma once
// HOST ONLY: OMS TPL mode 5 AES-128-CBC with IV from DLL primary address.
// Unlike AES-CCM, mode 5 DOES NOT authenticate the ciphertext. Even the
// 2F2F plausibility marker is not cryptographic proof of authenticity.
// Caller must separately verify complete DLL, identity and replay policy
// before considering this data. Never wire to ESPHome publications here.
#include <cstddef>
#include <cstdint>
#include <vector>
#include <openssl/evp.h>
namespace xmq_portable { namespace oms_cbc {
enum class Status : uint8_t {
  Ok, Invalid, UnsupportedCi, UnsupportedMode, UnsupportedBlockCount,
  MissingKey, BadMarker, CryptoError
};
struct Reading {
  std::vector<uint8_t> application;
  uint8_t tpl_status{0};
};
inline Status decode_short_tpl(const uint8_t *frame,size_t size,
                               const uint8_t *key,Reading &out) {
  out={};
  // Byte positions refer to the CRC-stripped 10-byte wireless DLL.
  if(!frame || size<15) return Status::Invalid;
  if(frame[10]!=0x7A) return Status::UnsupportedCi;
  const uint16_t cfg=static_cast<uint16_t>(frame[13]) |
      (static_cast<uint16_t>(frame[14])<<8);
  if(((cfg>>8)&0x1Fu)!=5) return Status::UnsupportedMode;
  const size_t blocks=static_cast<size_t>((cfg>>4)&0x0Fu);
  if(blocks==0 || blocks>15 || (cfg&0x000Fu)!=0)
    return Status::UnsupportedBlockCount;
  const size_t encrypted_length=blocks*16;
  if(size!=15+encrypted_length) return Status::UnsupportedBlockCount;
  if(!key) return Status::MissingKey;
  uint8_t iv[16]{};
  for(unsigned i=0;i<8;++i) iv[i]=frame[i+2];
  for(unsigned i=8;i<16;++i) iv[i]=frame[11];
  std::vector<uint8_t> plaintext(encrypted_length);
  EVP_CIPHER_CTX *ctx=EVP_CIPHER_CTX_new();
  if(!ctx) return Status::CryptoError;
  int written=0,tail=0;
  const bool ok=EVP_DecryptInit_ex(
       ctx,EVP_aes_128_cbc(),nullptr,key,iv)==1 &&
    EVP_CIPHER_CTX_set_padding(ctx,0)==1 &&
    EVP_DecryptUpdate(ctx,plaintext.data(),&written,
                      frame+15,static_cast<int>(encrypted_length))==1 &&
    static_cast<size_t>(written)==encrypted_length &&
    EVP_DecryptFinal_ex(ctx,plaintext.data()+written,&tail)==1 && tail==0;
  EVP_CIPHER_CTX_free(ctx);
  if(!ok) return Status::CryptoError;
  if(plaintext.size()<3 || plaintext[0]!=0x2F ||
     plaintext[1]!=0x2F || plaintext[2]!=0x0F) return Status::BadMarker;
  out.application.assign(plaintext.begin()+2,plaintext.end());
  out.tpl_status=frame[12];
  return Status::Ok;
}
}} // namespace xmq_portable::oms_cbc
