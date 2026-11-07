// Native encrypted Kamwater dispatcher (ESP-IDF-compatible mbedTLS).
// Synthetic AES-CTR ELL-II envelope with public NIST AES-128 key and
// a public MULTICAL21 compact TPL79 application. No production capture.
// DLL format-B CRC is independently verified before native dispatch.
#include "components/wmbus_native_sx126x/xmq_batch2_secure_dispatch.h"
#include "components/wmbus_native_sx126x/pure_complete_frame.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

int main() {
  const uint8_t key[16]={
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
    0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F};
  // AES-ECB keystream precomputed independently; session
  // 8D 32 01 03 00 00 20; ELL CRC recovered as 7F 17.
  const uint8_t ell[]={
    0x8D,0x32,0x01,0x03,0x00,0x00,0x20,
    0x9B,0xBC,0x37,0xD6,0x05,0x5E,0xA7,0x42,0x12,
    0x20,0x4D,0xE1,0x5E,0x99,0x47,0x2B,0x47,0x61,0xEC
  };
  std::vector<uint8_t> raw={
    0,0x44,0x2D,0x2C,0x78,0x56,0x34,0x12,0x1B,0x16
  };
  raw.insert(raw.end(),ell,ell+sizeof(ell));
  raw[0]=static_cast<uint8_t>(raw.size()+1);
  const auto crc=wmbus_pure::crc16_en13757(raw.data(),raw.size());
  raw.push_back(static_cast<uint8_t>(crc>>8));
  raw.push_back(static_cast<uint8_t>(crc));
  wmbus_pure::CompleteFrame verified{};
  assert(wmbus_pure::verify_complete_frame(
      raw.data(),raw.size(),wmbus_pure::CrcFrameFormat::B,verified)
      ==wmbus_pure::CompleteStatus::Ok);
  xmq_portable::CalcValue values[4]{};
  xmq_portable::CalendarDate dates[1]{};
  xmq_portable::DecodedPayload out{values,4,0,dates,1,0};
  using Status=xmq_portable::batch2_secure::Status;
  const auto decode=[&](const std::vector<uint8_t> &frame,
                        const char *configured,const uint8_t *k) {
    return xmq_portable::batch2_secure::decode_kamwater_ell_ctr(
        frame.data(),frame.size(),configured,k,out);
  };
  assert(decode(verified.bytes,"12345678",key)==Status::Ok);
  assert(out.value_count==2);
  assert(std::strcmp(out.values[0].name,"total_m3")==0);
  assert(std::fabs(out.values[0].value-6.408)<1e-9);
  assert(std::strcmp(out.values[1].name,"target_m3")==0);
  assert(std::fabs(out.values[1].value-6.408)<1e-9);
  // Wrong model, configured ID and changed session must fail closed.
  assert(decode(verified.bytes,"12345679",key)==Status::WrongMeter);
  assert(out.value_count==0);
  auto tampered=verified.bytes;
  tampered[8]=0x1A;
  assert(decode(tampered,"12345678",key)==Status::WrongModel);
  assert(out.value_count==0);
  tampered=verified.bytes;tampered[16]=0;
  assert(decode(tampered,"12345678",key)==
         Status::EncryptedDecodeRejected);
  assert(out.value_count==0);
  tampered=verified.bytes;tampered[13]^=1;
  assert(decode(tampered,"12345678",key)==
         Status::EncryptedDecodeRejected);
  assert(out.value_count==0);
  tampered=verified.bytes;tampered.back()^=1;
  assert(decode(tampered,"12345678",key)!=Status::Ok);
  assert(out.value_count==0);
  uint8_t wrong_key[16]{};std::memcpy(wrong_key,key,sizeof(key));
  wrong_key[0]^=1;
  assert(decode(verified.bytes,"12345678",wrong_key)!=Status::Ok);
  assert(out.value_count==0);
  assert(decode(verified.bytes,"12345678",nullptr)!=Status::Ok);
  assert(out.value_count==0);
  // Unverified input cannot pass on-air DLL integrity even if its
  // embedded application *might* look valid to a CRC-stripped decoder.
  auto broken=raw;broken[4]^=1;
  assert(wmbus_pure::verify_complete_frame(
      broken.data(),broken.size(),wmbus_pure::CrcFrameFormat::B,verified)
      !=wmbus_pure::CompleteStatus::Ok);
  std::cout<<"PASS native Kamwater mbedTLS CTR -> compact total/target "
              "6.408 m3, negatives, no live publication.\n";
  return 0;
}
