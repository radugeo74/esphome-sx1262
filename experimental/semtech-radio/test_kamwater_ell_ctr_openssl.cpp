#include "host_only/kamwater_ell_ctr_openssl.h"
#include <cassert>
#include <cstdint>
#include <vector>
int main() {
  using namespace xmq_portable::kamwater::ell_ctr;
  // Independent NIST AES-128 ECB known-answer check for the host backend.
  const uint8_t nist_key[16]={
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
    0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f
  };
  const uint8_t nist_plain[16]={
    0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
    0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff
  };
  const uint8_t nist_cipher[16]={
    0x69,0xc4,0xe0,0xd8,0x6a,0x7b,0x04,0x30,
    0xd8,0xcd,0xb7,0x80,0x70,0xb4,0xc5,0x5a
  };
  uint8_t block[16]{};
  assert(aes_ecb_block(nist_key,nist_plain,block));
  for(int i=0;i<16;++i) assert(block[i]==nist_cipher[i]);
  uint8_t counter[16]{};
  counter[15]=0xff;increment_counter(counter);
  assert(counter[14]==1&&counter[15]==0);
  // Fully synthetic ELL II + public compact application, no meter key.
  const uint8_t m[2]={0x2D,0x2C};
  const uint8_t a[6]={0x78,0x56,0x34,0x12,0x01,0x07};
  const uint8_t tpl[17]={
    0x79,0xED,0xA8,0x6A,0xB6,0x71,0x00,0x08,0x19,
    0x00,0x00,0x08,0x19,0x00,0x00,0x7F,0x13
  };
  std::vector<uint8_t> clear(2+sizeof(tpl));
  for(size_t i=0;i<sizeof(tpl);++i) clear[2+i]=tpl[i];
  const uint16_t crc=xmq_portable::kamwater::signature::crc16_en13757(
      tpl,sizeof(tpl));
  clear[0]=static_cast<uint8_t>(crc);
  clear[1]=static_cast<uint8_t>(crc>>8);
  std::vector<uint8_t> encrypted={0x8D,0x32,0x01,0x03,0,0,0x20};
  Iv iv{};
  assert(derive_iv(m,a,encrypted.data(),encrypted.size(),iv)==Status::Ok);
  for(size_t offset=0;offset<clear.size();offset+=16) {
    uint8_t stream[16]{};
    assert(aes_ecb_block(nist_key,iv.bytes,stream));
    const size_t n=clear.size()-offset<16?clear.size()-offset:16;
    for(size_t j=0;j<n;++j) encrypted.push_back(clear[offset+j]^stream[j]);
    increment_counter(iv.bytes);
  }
  std::vector<uint8_t> decoded{};
  assert(decrypt_ell_ii(m,a,nist_key,encrypted.data(),encrypted.size(),decoded)
         ==DecryptStatus::Ok);
  assert(decoded.size()==sizeof(tpl));
  for(size_t i=0;i<sizeof(tpl);++i)assert(decoded[i]==tpl[i]);
  // Wrong key, corrupted payload, wrong mode and truncated ciphertext
  // must not publish any decrypted output.
  auto key_wrong=std::vector<uint8_t>(nist_key,nist_key+16);
  key_wrong[0]^=1;
  assert(decrypt_ell_ii(m,a,key_wrong.data(),encrypted.data(),
                       encrypted.size(),decoded)==DecryptStatus::BadCrc);
  assert(decoded.empty());
  auto bad=encrypted;
  bad.back()^=1;
  assert(decrypt_ell_ii(m,a,nist_key,bad.data(),bad.size(),decoded)
         ==DecryptStatus::BadCrc);
  assert(decoded.empty());
  bad=encrypted;bad[6]=0;
  assert(decrypt_ell_ii(m,a,nist_key,bad.data(),bad.size(),decoded)
         ==DecryptStatus::Unsupported);
  assert(decoded.empty());
  assert(decrypt_ell_ii(m,a,nist_key,encrypted.data(),8,decoded)
         ==DecryptStatus::Invalid);
  assert(decoded.empty());
  // Changed hop/repeated flags preserve the IV and valid decryption.
  bad=encrypted;bad[1]=0x20;
  assert(decrypt_ell_ii(m,a,nist_key,bad.data(),bad.size(),decoded)
         ==DecryptStatus::Ok);
  assert(decoded.size()==sizeof(tpl));
  return 0;
}
