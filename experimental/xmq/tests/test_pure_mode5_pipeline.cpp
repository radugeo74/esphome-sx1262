#include "../../../components/wmbus_radio/pure_mode5_decrypt.h"
#include <openssl/aes.h>
#include "../../../components/wmbus_meter/xmq_decode_catalog.h"
#include <cmath>
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
  // A synthetic encrypted envelope carrying a known mkradio4 test payload.
  // This tests plumbing, not compatibility with a real encrypted meter.
  uint8_t clear[32]{
    0x2f,0x2f,0x06,0x70,0x29,0x01,0x00,0x60,0x17,
    0x03,0x00,0x04,0x00,0x03,0x00,0x00,0x00
  };
  uint8_t frame[11+4+32]{};
  frame[0] = static_cast<uint8_t>(sizeof(frame)-1);
  frame[1]=0x44;frame[2]=0x68;frame[3]=0x50;frame[4]=0x01;frame[5]=0x00;frame[6]=0x00;frame[7]=0x31;frame[8]=0x69;
  frame[9]=0x80;frame[10]=0x7A;
  frame[11]=0x2A;frame[12]=0;frame[13]=0x20;frame[14]=0x05;
  Header header{};
  assert(parse_header(frame,sizeof(frame),header)==HeaderStatus::Ok);
  TransportEnvelope transport{};
  assert(inspect_transport(header,transport)==TransportStatus::Ok);
  Mode5Prepared prepared{};
  assert(prepare_mode5(frame,sizeof(frame),header,transport,prepared)==Mode5PrepareStatus::Ok);
  // Mode 5 must not prepare an empty encrypted-block declaration.
  // This check is independent from AES and publishing.
  uint8_t zero_block_frame[sizeof(frame)]{};
  std::memcpy(zero_block_frame, frame, sizeof(frame));
  zero_block_frame[13]=0x00;
  assert(parse_header(zero_block_frame,sizeof(zero_block_frame),header)==HeaderStatus::Ok);
  assert(inspect_transport(header,transport)==TransportStatus::Ok);
  Mode5Prepared rejected{};
  assert(prepare_mode5(zero_block_frame,sizeof(zero_block_frame),
                       header,transport,rejected)==Mode5PrepareStatus::InvalidBlockCount);
  assert(rejected.encrypted==nullptr && rejected.encrypted_size==0);
  // Restore the original frame view for the normal CBC fixture.
  assert(parse_header(frame,sizeof(frame),header)==HeaderStatus::Ok);
  assert(inspect_transport(header,transport)==TransportStatus::Ok);
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
  // New configured-meter wrapper: validate ID, then decrypt in one step.
  std::vector<uint8_t> gated_plaintext;
  assert(decrypt_configured_mode5(frame,sizeof(frame),31000001u,key,
                                  block,nullptr,gated_plaintext)
         == ConfiguredMode5DecryptStatus::Ok);
  assert(gated_plaintext.size()==sizeof(clear));
  assert(std::memcmp(gated_plaintext.data(),clear,sizeof(clear))==0);
  // Full synthetic Mode5 -> existing AES -> generated XMQ driver.
  // No legacy handleTelegram, no sensor publishing and no key logging.
  const auto *driver=xmq_portable::find_decode_descriptor("mkradio4",0xA2);
  assert(driver);
  xmq_portable::Capture captures[12]{};
  xmq_portable::CalcValue values[24]{};
  xmq_portable::CalendarDate dates[2]{};
  xmq_portable::DecodedPayload decoded{values,24,0,dates,2,0};
  assert(xmq_portable::decode_mode5_plain_payload(
      *driver,gated_plaintext.data(),gated_plaintext.size(),
      captures,12,decoded)==xmq_portable::DriverStatus::Ok);
  bool target=false,total=false;
  for (size_t i=0;i<decoded.value_count;++i) {
    if (!decoded.values[i].name) continue;
    if (std::strcmp(decoded.values[i].name,"target")==0) {
      assert(std::fabs(decoded.values[i].value-0.1)<1e-6);
      target=true;
    }
    if (std::strcmp(decoded.values[i].name,"total")==0) {
      assert(std::fabs(decoded.values[i].value-0.4)<1e-6);
      total=true;
    }
  }
  assert(target && total);

  assert(decrypt_configured_mode5(frame,sizeof(frame),31000002u,key,
                                  block,nullptr,gated_plaintext)
         == ConfiguredMode5DecryptStatus::PreparationFailed);
  assert(gated_plaintext.empty());
  // Recovery must work after a rejected frame, with no cached plaintext.
  assert(decrypt_configured_mode5(frame,sizeof(frame),31000001u,key,
                                  block,nullptr,gated_plaintext)
         == ConfiguredMode5DecryptStatus::Ok);
  assert(gated_plaintext.size()==sizeof(clear));
  frame[15]^=0x01;
  assert(decrypt_mode5(prepared,key,block,nullptr,plaintext)==Mode5DecryptStatus::InvalidPlaintextMarker);
  assert(plaintext.empty());
  assert(decrypt_configured_mode5(frame,sizeof(frame),31000001u,key,
                                  block,nullptr,gated_plaintext)
         == ConfiguredMode5DecryptStatus::DecryptionFailed);
  assert(gated_plaintext.empty());
  // Header/config inconsistencies must never reach decrypt.
  frame[13]=0x40;
  assert(parse_header(frame,sizeof(frame),header)==HeaderStatus::Ok);
  assert(inspect_transport(header,transport)==TransportStatus::Ok);
  assert(prepare_mode5(frame,sizeof(frame),header,transport,prepared)==Mode5PrepareStatus::TruncatedPayload);
}
