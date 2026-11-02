#include "../../../components/wmbus_radio/pure_mode5_decrypt.h"
#include <openssl/aes.h>
#include <cassert>
#include <cstring>
#include <vector>
using namespace wmbus_pure;
static bool block(const uint8_t key[16], const uint8_t in[16], uint8_t out[16], void *) {
  AES_KEY aes{};
  if (AES_set_decrypt_key(key, 128, &aes) != 0) return false;
  AES_decrypt(in, out, &aes);
  return true;
}
int main() {
  // Synthetic complete, CRC-free DLL frame using CI=7A and mode 5.
  // All values and keys are public test data, never real meter secrets.
  const uint8_t key[16] = {0x2b,0x7e,0x15,0x16,0x28,0xae,0xd2,0xa6,
                           0xab,0xf7,0x15,0x88,0x09,0xcf,0x4f,0x3c};
  uint8_t clear[32]{0x2f,0x2f,0x02,0x13,0x64,0x00};
  uint8_t frame[11+4+32]{};
  frame[0] = static_cast<uint8_t>(sizeof(frame)-1);
  frame[1]=0x44;frame[2]=0x68;frame[3]=0x50;frame[4]=0x67;
  frame[5]=0x30;frame[6]=0x27;frame[7]=0x24;frame[8]=0x69;
  frame[9]=0x80;frame[10]=0x7A;
  frame[11]=0x2A;frame[12]=0;frame[13]=0x20;frame[14]=0x05;
  Header header{};
  assert(parse_header(frame,sizeof(frame),header)==HeaderStatus::Ok);
  TransportEnvelope transport{};
  assert(inspect_transport(header,transport)==TransportStatus::Ok);
  Mode5Prepared prepared{};
  assert(prepare_mode5(frame,sizeof(frame),header,transport,prepared)==Mode5PrepareStatus::Ok);
  AES_KEY aes{};
  assert(AES_set_encrypt_key(key,128,&aes)==0);
  uint8_t chain[16]{};
  std::memcpy(chain,prepared.iv,16);
  for(size_t offset=0;offset<32;offset+=16) {
    uint8_t input[16]{};
    for(size_t i=0;i<16;++i) input[i]=static_cast<uint8_t>(clear[offset+i]^chain[i]);
    AES_encrypt(input,frame+15+offset,&aes);
    std::memcpy(chain,frame+15+offset,16);
  }
  std::vector<uint8_t> plaintext;
  assert(decrypt_mode5(prepared,key,block,nullptr,plaintext)==Mode5DecryptStatus::Ok);
  assert(plaintext.size()==sizeof(clear) && std::memcmp(plaintext.data(),clear,32)==0);
  frame[15]^=0x01;
  assert(decrypt_mode5(prepared,key,block,nullptr,plaintext)==Mode5DecryptStatus::InvalidPlaintextMarker);
  assert(plaintext.empty());
  // Header/config inconsistencies must never reach decrypt.
  frame[13]=0x40;
  assert(parse_header(frame,sizeof(frame),header)==HeaderStatus::Ok);
  assert(inspect_transport(header,transport)==TransportStatus::Ok);
  assert(prepare_mode5(frame,sizeof(frame),header,transport,prepared)==Mode5PrepareStatus::TruncatedPayload);
}
