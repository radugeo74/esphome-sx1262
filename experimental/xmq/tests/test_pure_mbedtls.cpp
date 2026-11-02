#include "../../../components/wmbus_radio/pure_aes_mbedtls.h"
#include "../../../components/wmbus_radio/pure_aes_cbc.h"
#include "../../../components/wmbus_radio/pure_mode5_decrypt.h"
#include "../../../components/wmbus_meter/xmq_decode_catalog.h"
#include <cmath>
#include <cassert>
#include <cstring>
#include <vector>
using namespace wmbus_pure;
int main() {
  // NIST SP 800-38A F.2.1, first block AES-128-CBC.
  const uint8_t key[16]={0x2b,0x7e,0x15,0x16,0x28,0xae,0xd2,0xa6,
    0xab,0xf7,0x15,0x88,0x09,0xcf,0x4f,0x3c};
  const uint8_t iv[16]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
  const uint8_t cipher[16]={0x76,0x49,0xab,0xac,0x81,0x19,0xb2,0x46,
    0xce,0xe9,0x8e,0x9b,0x12,0xe9,0x19,0x7d};
  const uint8_t expected[16]={0x6b,0xc1,0xbe,0xe2,0x2e,0x40,0x9f,0x96,
    0xe9,0x3d,0x7e,0x11,0x73,0x93,0x17,0x2a};
  std::vector<uint8_t> result;
  assert(decrypt_aes128_cbc(cipher,16,key,iv,mbedtls_aes128_decrypt_block,
                             nullptr,result)==AesCbcStatus::Ok);
  assert(result.size()==16 && std::memcmp(result.data(),expected,16)==0);
  // ESP-IDF mbedTLS adapter, exercised through the full configured Mode-5
  // entry point rather than only through the raw CBC helper.
  uint8_t frame[11+4+32]{};
  frame[0]=static_cast<uint8_t>(sizeof(frame)-1);
  frame[1]=0x44; frame[2]=0x68; frame[3]=0x50;
  frame[4]=0x01; frame[5]=0x00; frame[6]=0x00;
  frame[7]=0x31; frame[8]=0x69; frame[9]=0x80;
  frame[10]=0x7A; frame[11]=0x2A; frame[12]=0;
  frame[13]=0x20; frame[14]=0x05;
  Mode5Prepared prepared{};
  assert(prepare_configured_mode5(frame,sizeof(frame),31000001u,prepared)
         == ConfiguredMode5Status::Ok);
  const uint8_t clear[32] = {0x2F,0x2F,0x06,0x70,0x29,0x01,0x00,0x60,0x17,
                             0x03,0x00,0x04,0x00,0x03,0x00,0x00,0x00};
  mbedtls_aes_context enc;
  mbedtls_aes_init(&enc);
  assert(mbedtls_aes_setkey_enc(&enc,key,128)==0);
  uint8_t chain[16]{};
  std::memcpy(chain,prepared.iv,sizeof(chain));
  for(size_t offset=0; offset<sizeof(clear); offset+=16) {
    uint8_t input[16]{};
    for(size_t i=0;i<16;++i) input[i]=clear[offset+i]^chain[i];
    assert(mbedtls_aes_crypt_ecb(&enc,MBEDTLS_AES_ENCRYPT,input,
                                  frame+15+offset)==0);
    std::memcpy(chain,frame+15+offset,sizeof(chain));
  }
  mbedtls_aes_free(&enc);
  std::vector<uint8_t> decoded;
  assert(decrypt_configured_mode5(frame,sizeof(frame),31000001u,key,
                                  mbedtls_aes128_decrypt_block,nullptr,decoded)
         == ConfiguredMode5DecryptStatus::Ok);
  assert(decoded.size()==sizeof(clear));
  assert(std::memcmp(decoded.data(),clear,sizeof(clear))==0);
  // Feed the decrypted application to the generated XMQ water decoder.
  // Synthetic fixture: validates handoff only, not live encrypted meters.
  const auto *water = xmq_portable::find_decode_descriptor("mkradio4",0xA2);
  assert(water != nullptr);
  xmq_portable::Capture captures[12]{};
  xmq_portable::CalcValue values[24]{};
  xmq_portable::CalendarDate dates[2]{};
  xmq_portable::DecodedPayload output{values,24,0,dates,2,0};
  assert(xmq_portable::decode_mode5_plain_payload(*water,decoded.data(),
      decoded.size(),captures,12,output)==xmq_portable::DriverStatus::Ok);
  bool found_target=false,found_total=false;
  for(size_t i=0;i<output.value_count;++i) {
    if(!output.values[i].name) continue;
    if(std::strcmp(output.values[i].name,"target")==0) {
      assert(std::fabs(output.values[i].value-0.1)<1e-6);
      found_target=true;
    }
    if(std::strcmp(output.values[i].name,"total")==0) {
      assert(std::fabs(output.values[i].value-0.4)<1e-6);
      found_total=true;
    }
  }
  assert(found_target && found_total);

  assert(decrypt_configured_mode5(frame,sizeof(frame),31000002u,key,
                                  mbedtls_aes128_decrypt_block,nullptr,decoded)
         == ConfiguredMode5DecryptStatus::PreparationFailed);
  assert(decoded.empty());
  assert(decrypt_configured_mode5(frame,sizeof(frame),31000001u,key,
                                  mbedtls_aes128_decrypt_block,nullptr,decoded)
         == ConfiguredMode5DecryptStatus::Ok);
  frame[15]^=1;
  assert(decrypt_configured_mode5(frame,sizeof(frame),31000001u,key,
                                  mbedtls_aes128_decrypt_block,nullptr,decoded)
         == ConfiguredMode5DecryptStatus::DecryptionFailed);
  assert(decoded.empty());
  // An invalid encrypted packet cannot be decoded using a cached result.
  assert(xmq_portable::decode_mode5_plain_payload(*water,
      decoded.data(),decoded.size(),captures,12,output)
      == xmq_portable::DriverStatus::BadArguments);
  assert(output.value_count==0 && output.date_count==0);

}
