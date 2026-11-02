#include "../../../components/wmbus_radio/pure_mode5_decrypt.h"
#include <openssl/aes.h>
#include <cassert>
#include <cstring>
using namespace wmbus_pure;
static bool decrypt_block(const uint8_t key[16],const uint8_t in[16],
                          uint8_t out[16],void *) {
  AES_KEY schedule{};
  if (AES_set_decrypt_key(key,128,&schedule)!=0) return false;
  AES_decrypt(in,out,&schedule);
  return true;
}
static bool reject_block(const uint8_t[16],const uint8_t[16],uint8_t[16],void *) {
  return false;
}
int main() {
  const uint8_t key[16]={0x2b,0x7e,0x15,0x16,0x28,0xae,0xd2,0xa6,
    0xab,0xf7,0x15,0x88,0x09,0xcf,0x4f,0x3c};
  const uint8_t iv[16]={0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
    0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f};
  uint8_t expected[32]={0x2f,0x2f};
  for(size_t i=2;i<sizeof(expected);++i) expected[i]=static_cast<uint8_t>(i+1);
  AES_KEY encrypt_schedule{};
  assert(AES_set_encrypt_key(key,128,&encrypt_schedule)==0);
  uint8_t c[32]{},prev[16];
  std::memcpy(prev,iv,16);
  for(size_t offset=0;offset<32;offset+=16) {
    uint8_t block[16];
    for(size_t j=0;j<16;++j)block[j]=expected[offset+j]^prev[j];
    AES_encrypt(block,c+offset,&encrypt_schedule);
    std::memcpy(prev,c+offset,16);
  }
  Mode5Prepared prepared{};
  prepared.encrypted=c;prepared.encrypted_size=sizeof(c);
  std::memcpy(prepared.iv,iv,16);
  std::vector<uint8_t> plain;
  assert(decrypt_mode5(prepared,key,decrypt_block,nullptr,plain)==Mode5DecryptStatus::Ok);
  assert(plain.size()==sizeof(expected) && std::memcmp(plain.data(),expected,32)==0);
  assert(decrypt_mode5(prepared,key,reject_block,nullptr,plain)==Mode5DecryptStatus::CryptoFailure);
  assert(plain.empty());
  c[0]^=0x01;
  assert(decrypt_mode5(prepared,key,decrypt_block,nullptr,plain)==Mode5DecryptStatus::InvalidPlaintextMarker);
  assert(plain.empty());
  c[0]^=0x01;
  prepared.encrypted_size=17;
  assert(decrypt_mode5(prepared,key,decrypt_block,nullptr,plain)==Mode5DecryptStatus::InvalidPreparation);
  assert(plain.empty());
}
