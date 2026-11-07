// Runs the EXACT native AES-128-CBC mbedTLS decoder and Apator162 XMQ
// application parser against public upstream encrypted fixture #19.
// The only synthetic bytes are the on-air DLL format-B CRC trailer.
// HOST execution; not an ESP32 device RF or Home Assistant test.
#include "components/wmbus_native_sx126x/xmq_batch2_secure_dispatch.h"
#include "components/wmbus_native_sx126x/pure_complete_frame.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

static std::vector<uint8_t> unhex(const char *s) {
  std::vector<uint8_t> bytes;int first=-1;
  for(;*s;++s) {
    const char c=*s;
    const int n=c>='0'&&c<='9'?c-'0':
      c>='a'&&c<='f'?c-'a'+10:
      c>='A'&&c<='F'?c-'A'+10:-1;
    assert(n>=0);
    if(first<0) first=n;
    else {bytes.push_back(static_cast<uint8_t>((first<<4)|n));first=-1;}
  }
  assert(first<0);
  return bytes;
}
int main() {
  const auto source=unhex(
      "3e4401069620280105077a7400308588f90b4dbd012bd789d7b4fb8536"
      "b889425b8624aee20a4862e60223427e24a9060d0db7b112cdd42a0"
      "a825130f3858a");
  // The published XMQ #19 decryption key is all-zero, not a user key.
  const uint8_t key[16]{};
  using namespace xmq_portable;
  CalcValue values[4]{};
  CalendarDate dates[1]{};
  DecodedPayload decoded{values,4,0,dates,1,0};
  auto full=source;
  full[0]=static_cast<uint8_t>(full.size()+1u);
  const auto crc=wmbus_pure::crc16_en13757(full.data(),full.size());
  full.push_back(static_cast<uint8_t>(crc>>8));
  full.push_back(static_cast<uint8_t>(crc));
  wmbus_pure::CompleteFrame checked{};
  using FrameStatus=wmbus_pure::CompleteStatus;
  assert(wmbus_pure::verify_complete_frame(
      full.data(),full.size(),wmbus_pure::CrcFrameFormat::B,checked)
      ==FrameStatus::Ok);
  assert(checked.bytes==source);
  using Decode=batch2_secure::Status;
  const auto decode=[&](const std::vector<uint8_t> &bytes,
                        const char *id,const uint8_t *secret) {
    return batch2_secure::decode_apator162_mode5(
        bytes.data(),bytes.size(),id,secret,decoded);
  };
  assert(decode(checked.bytes,"01282096",key)==Decode::Ok);
  assert(decoded.value_count==1 && decoded.values[0].name!=nullptr);
  assert(std::strcmp(decoded.values[0].name,"total_m3")==0);
  assert(std::fabs(decoded.values[0].value-7.512)<1e-9);
  // A wrong independently configured meter must never obtain a reading.
  assert(decode(checked.bytes,"01282097",key)==Decode::WrongMeter);
  assert(decoded.value_count==0 && decoded.date_count==0);
  assert(decode(checked.bytes,"01282096",key)==Decode::Ok);
  uint8_t wrong_key[16]{};wrong_key[0]=0x01;
  assert(decode(checked.bytes,"01282096",wrong_key)!=Decode::Ok);
  assert(decoded.value_count==0);
  assert(decode(checked.bytes,"01282096",nullptr)!=Decode::Ok);
  assert(decoded.value_count==0);
  auto altered=checked.bytes;
  altered[14]=0; // security mode 0 cannot use CBC decoder
  assert(decode(altered,"01282096",key)==Decode::EncryptedDecodeRejected);
  assert(decoded.value_count==0);
  altered=checked.bytes;
  altered.pop_back();
  altered[0]=static_cast<uint8_t>(altered.size()-1);
  assert(decode(altered,"01282096",key)!=Decode::Ok);
  assert(decoded.value_count==0);
  altered=checked.bytes;
  altered[9]=0x08; // not an official Apator162 water type
  assert(decode(altered,"01282096",key)==Decode::WrongModel);
  assert(decoded.value_count==0);
  altered=checked.bytes;
  altered[15]^=1; // corrupt initial CBC block, no 2F2F/0F plausibility
  assert(decode(altered,"01282096",key)!=Decode::Ok);
  assert(decoded.value_count==0);
  // Full DLL integrity must be independently checked BEFORE decrypting.
  auto damaged=full;
  damaged[6]^=1;
  assert(wmbus_pure::verify_complete_frame(
      damaged.data(),damaged.size(),wmbus_pure::CrcFrameFormat::B,checked)
      !=FrameStatus::Ok);
  std::cout<<"PASS Apator162 native mbedTLS AES-CBC #19 -> 7.512 m3; "
              "bad key/identity/mode/DLL rejected. No RF/HA publication.\n";
  return 0;
}
