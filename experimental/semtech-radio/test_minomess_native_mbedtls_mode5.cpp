// Minomess long-TPL encrypted application through ACTUAL native mbedTLS
// and XMQ parser. AES-CBC ciphertext is synthesized independently from
// original public Minomess clear source data with a NIST AES example key.
// Original upstream NOKEY plaintext is NEVER treated as encrypted RF.
#include "components/wmbus_native_sx126x/xmq_batch2_secure_dispatch.h"
#include "components/wmbus_native_sx126x/pure_complete_frame.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>
static std::vector<uint8_t> from_hex(const char *p) {
  std::vector<uint8_t> result;int hi=-1;
  for(;*p;++p) {
    const char c=*p;
    const int v=c>='0'&&c<='9'?c-'0':
      c>='A'&&c<='F'?c-'A'+10:
      c>='a'&&c<='f'?c-'a'+10:-1;
    assert(v>=0);
    if(hi<0) hi=v;
    else {result.push_back(static_cast<uint8_t>((hi<<4)|v));hi=-1;}
  }
  assert(hi<0);
  return result;
}
int main() {
  // Original upstream application, with independently AES-CBC-encrypted
  // 80-byte payload and original long TPL [ID,ZRI,01,06,ACC=7D,0550].
  const auto encrypted=from_hex(
    "6644496a7856341218377278563412496a01067d005005"
    "31d440266572deabc2b5912e0a9e673eb6f8b0bd1dcf181b4246188a4b42a8a"
    "4d7d072ea4b0b2dfcb3f5183b36ce5bf0d746bd1a818f00eb2b31a4c751f596"
    "e6fd0918ca738fd0a11d8fdd6534d83db8");
  assert(encrypted.size()==size_t(encrypted[0])+1);
  const uint8_t fixture_key[16]={
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
    0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F};
  auto rf=encrypted;
  rf[0]=static_cast<uint8_t>(rf.size()+1);
  const uint16_t crc=wmbus_pure::crc16_en13757(rf.data(),rf.size());
  rf.push_back(static_cast<uint8_t>(crc>>8));
  rf.push_back(static_cast<uint8_t>(crc));
  wmbus_pure::CompleteFrame verified{};
  assert(wmbus_pure::verify_complete_frame(
      rf.data(),rf.size(),wmbus_pure::CrcFrameFormat::B,verified)
      ==wmbus_pure::CompleteStatus::Ok);
  xmq_portable::CalcValue values[4]{};
  xmq_portable::CalendarDate dates[2]{};
  xmq_portable::DecodedPayload decoded{values,4,0,dates,2,0};
  using Status=xmq_portable::batch2_secure::Status;
  const auto decode=[&](const std::vector<uint8_t> &frame,
                        const char *id,const uint8_t *key) {
    return xmq_portable::batch2_secure::decode_minomess_mode5(
        frame.data(),frame.size(),id,key,decoded);
  };
  assert(decode(verified.bytes,"12345678",fixture_key)==Status::Ok);
  assert(decoded.value_count==2);
  assert(std::strcmp(decoded.values[0].name,"total_m3")==0);
  assert(std::fabs(decoded.values[0].value-140.354)<1e-9);
  assert(std::strcmp(decoded.values[1].name,"target_m3")==0);
  assert(std::fabs(decoded.values[1].value-138.485)<1e-9);
  assert(decode(verified.bytes,"12345679",fixture_key)==Status::WrongMeter);
  assert(decoded.value_count==0);
  uint8_t wrong[16]{};std::memcpy(wrong,fixture_key,sizeof(wrong));
  wrong[0]^=1;
  assert(decode(verified.bytes,"12345678",wrong)!=Status::Ok);
  assert(decoded.value_count==0);
  assert(decode(verified.bytes,"12345678",nullptr)!=Status::Ok);
  assert(decoded.value_count==0);
  auto damaged=verified.bytes;
  damaged[14]^=1; // TPL meter ID diverges from configured DLL ID
  assert(decode(damaged,"12345678",fixture_key)==Status::WrongModel);
  assert(decoded.value_count==0);
  damaged=verified.bytes;
  damaged[22]=0x00; // no mode5; do NOT interpret encrypted bytes
  assert(decode(damaged,"12345678",fixture_key)==
         Status::EncryptedDecodeRejected);
  assert(decoded.value_count==0);
  damaged=verified.bytes;
  damaged[23]^=1; // first CBC ciphertext block corrupt
  assert(decode(damaged,"12345678",fixture_key)!=Status::Ok);
  assert(decoded.value_count==0);
  // Original public upstream NOKEY application bytes with Mode-5 config
  // are intentionally not a real encrypted RF frame: must be rejected.
  const auto supplied_clear=from_hex(
    "6644496A7856341218377278563412496A01067D0050052F2F"
    "0C1354031400026C4F3A82046C413A8C0413F51C0200"
    "8D0493132C33FEEE0F020D05021EFA018CED010CE0013ED401"
    "FFC601A9B701E2A2011B94014884011D7401FC6601EA5A01"
    "02FD1700002F2F");
  assert(decode(supplied_clear,"12345678",fixture_key)!=Status::Ok);
  assert(decoded.value_count==0);
  damaged=rf;damaged[5]^=1; // invalid full DLL CRC
  assert(wmbus_pure::verify_complete_frame(
      damaged.data(),damaged.size(),wmbus_pure::CrcFrameFormat::B,verified)
      !=wmbus_pure::CompleteStatus::Ok);
  std::cout<<"PASS native Minomess mbedTLS long-TPL Mode5: "
              "140.354 / 138.485 m3, negative transport/security, no live RF.\n";
  return 0;
}
