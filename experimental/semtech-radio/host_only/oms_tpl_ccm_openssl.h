#pragma once
// Host-only OMS security profile D (TPL CI 7A, security mode 10).
// Based on wmbusmeters parseTPLConfig and decrypt_TPL_AES_CCM.
// Strict subset: no partial encryption, short TPL only, KDF 1 only.
// All data is withheld unless OpenSSL verifies the CCM authentication tag.
// Synthetic tests only; no firmware integration or real meter credentials.
#include <cstddef>
#include <cstdint>
#include <vector>
#include <openssl/evp.h>
namespace xmq_portable { namespace oms_ccm {
enum class Status : uint8_t {
  Ok, Invalid, UnsupportedCi, UnsupportedMode, UnsupportedKdf,
  UnsupportedPartial, MissingKey, CryptoError, BadTag
};
struct Output {
  std::vector<uint8_t> clear_application;
  uint8_t tpl_status{0};
  uint32_t counter{0};
};
inline bool derive_kenc(const uint8_t master_key[16],
                        const uint8_t counter_le[4],
                        const uint8_t meter_id_le[4],
                        uint8_t derived[16]) {
  if(!master_key||!counter_le||!meter_id_le||!derived) return false;
  const uint8_t input[16] = {
    0x00,counter_le[0],counter_le[1],counter_le[2],counter_le[3],
    meter_id_le[0],meter_id_le[1],meter_id_le[2],meter_id_le[3],
    7,7,7,7,7,7,7
  };
  EVP_MAC *mac=EVP_MAC_fetch(nullptr,"CMAC",nullptr);
  if(!mac) return false;
  EVP_MAC_CTX *ctx=EVP_MAC_CTX_new(mac);
  EVP_MAC_free(mac);
  if(!ctx) return false;
  const char cipher[]="AES-128-CBC";
  OSSL_PARAM params[]={
    OSSL_PARAM_construct_utf8_string("cipher",const_cast<char *>(cipher),0),
    OSSL_PARAM_construct_end()
  };
  size_t out_size=0;
  const bool ok=EVP_MAC_init(ctx,master_key,16,params)==1 &&
    EVP_MAC_update(ctx,input,sizeof(input))==1 &&
    EVP_MAC_final(ctx,derived,&out_size,16)==1 && out_size==16;
  EVP_MAC_CTX_free(ctx);
  return ok;
}
inline Status decode_short_tpl(const uint8_t dll_m[2],
                              const uint8_t dll_id[4],
                              uint8_t version,uint8_t medium,
                              const uint8_t master_key[16],
                              const uint8_t *tpl,size_t length,
                              Output &out) {
  out={};
  if(!dll_m || !dll_id || !tpl || length<16) return Status::Invalid;
  if(tpl[0]!=0x7A) return Status::UnsupportedCi;
  const uint16_t cfg=static_cast<uint16_t>(tpl[3]) |
      (static_cast<uint16_t>(tpl[4])<<8);
  const uint8_t mode=static_cast<uint8_t>((cfg>>8)&31u);
  if(mode!=10) return Status::UnsupportedMode;
  if((cfg&0xFF)!=0xFF) return Status::UnsupportedPartial;
  const uint16_t cfe=static_cast<uint16_t>(tpl[5]) |
      (static_cast<uint16_t>(tpl[6])<<8);
  const uint8_t kdf=static_cast<uint8_t>((cfe>>4)&3u);
  if(kdf!=1) return Status::UnsupportedKdf;
  const uint8_t tag_size=static_cast<uint8_t>(4+4*((cfe>>8)&3u));
  const bool has_version=((cfe>>6)&1u)!=0;
  const size_t counter_offset=7+(has_version?1:0);
  const size_t payload_offset=counter_offset+4;
  if(length<=payload_offset+tag_size) return Status::Invalid;
  if(!master_key) return Status::MissingKey;
  const uint8_t *counter=tpl+counter_offset;
  uint8_t key[16]{};
  if(!derive_kenc(master_key,counter,dll_id,key)) return Status::CryptoError;
  // Original nonce: M(2), ID(4), version, medium, 00, counter BE(4).
  const uint8_t nonce[13]={
    dll_m[0],dll_m[1],dll_id[0],dll_id[1],dll_id[2],dll_id[3],
    version,medium,0,
    counter[3],counter[2],counter[1],counter[0]
  };
  const size_t ciphertext_length=length-payload_offset-tag_size;
  // OpenSSL EVP CCM verifies the tag in DecryptUpdate; no partial output.
  EVP_CIPHER_CTX *ctx=EVP_CIPHER_CTX_new();
  if(!ctx) return Status::CryptoError;
  std::vector<uint8_t> candidate(ciphertext_length);
  int written=0;
  const bool setup=
    EVP_DecryptInit_ex(ctx,EVP_aes_128_ccm(),nullptr,nullptr,nullptr)==1 &&
    EVP_CIPHER_CTX_ctrl(ctx,EVP_CTRL_CCM_SET_IVLEN,13,nullptr)==1 &&
    EVP_CIPHER_CTX_ctrl(ctx,EVP_CTRL_CCM_SET_TAG,tag_size,
                        const_cast<uint8_t *>(tpl+length-tag_size))==1 &&
    EVP_DecryptInit_ex(ctx,nullptr,nullptr,key,nonce)==1 &&
    EVP_DecryptUpdate(ctx,nullptr,&written,nullptr,
                      static_cast<int>(ciphertext_length))==1 &&
    EVP_DecryptUpdate(ctx,nullptr,&written,tpl,
                      static_cast<int>(counter_offset))==1;
  const bool authentic=setup &&
    EVP_DecryptUpdate(ctx,candidate.data(),&written,
                      tpl+payload_offset,
                      static_cast<int>(ciphertext_length))==1 &&
    static_cast<size_t>(written)==ciphertext_length;
  EVP_CIPHER_CTX_free(ctx);
  if(!authentic) return setup?Status::BadTag:Status::CryptoError;
  out.clear_application=std::move(candidate);
  out.tpl_status=tpl[2];
  out.counter=static_cast<uint32_t>(counter[0])|
       (static_cast<uint32_t>(counter[1])<<8)|
       (static_cast<uint32_t>(counter[2])<<16)|
       (static_cast<uint32_t>(counter[3])<<24);
  return Status::Ok;
}
}} // namespace xmq_portable::oms_ccm
