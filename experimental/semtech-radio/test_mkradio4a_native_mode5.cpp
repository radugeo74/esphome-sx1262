// MK Radio 4a: independently AES-encrypted public original XMQ
// application records, with TCH DLL converter and HYD TPL address.
// The original upstream NOKEY bytes are NOT treated as live ciphertext.
// Synthetic complete format-B CRC envelope validates the radio ingress.
#include "components/wmbus_native_sx126x/xmq_batch2_secure_dispatch.h"
#include "components/wmbus_native_sx126x/pure_complete_frame.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>
static std::vector<uint8_t> hex(const char *p) {
  std::vector<uint8_t> v;int nib=-1;
  while(*p) {
    const char c=*p++;
    const int d=c>='0'&&c<='9'?c-'0':
      c>='A'&&c<='F'?c-'A'+10:
      c>='a'&&c<='f'?c-'a'+10:-1;
    assert(d>=0);
    if(nib<0)nib=d;
    else {v.push_back(static_cast<uint8_t>((nib<<4)|d));nib=-1;}
  }
  assert(nib<0);return v;
}
int main() {
  // Synthesized from source fixture #1: original clear 16.1 m3 and
  // 2021-12-31 storage-one date; public NIST example AES key.
  // CBC IV = HYD(2423), TPL 66953825, FE06, access 4E repeated.
  // Original Mode-5 0530 encrypts exactly 3 blocks; remaining 5 bytes
  // are untrusted trailer, never interpreted as measurements.
  const auto encrypted=hex(
    "4B44685036494600953772253895662423FE064E003005"
    "DF5266AD7770799E99223CADB3EBD2144CD83D771F47A1DC0645E1D42F"
    "291D89ED6E6449DEC640AC2C637FFB8F26BB242F2F2F2F2F");
  assert(encrypted.size()==76 && encrypted[0]==75);
  const uint8_t key[16]={
      0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
  // Explicitly CRC-verified B packet before running native secure path.
  auto air=encrypted;
  air[0]=static_cast<uint8_t>(air.size()+1);
  const auto crc=wmbus_pure::crc16_en13757(air.data(),air.size());
  air.push_back(static_cast<uint8_t>(crc>>8));
  air.push_back(static_cast<uint8_t>(crc));
  wmbus_pure::CompleteFrame checked{};
  assert(wmbus_pure::verify_complete_frame(
    air.data(),air.size(),wmbus_pure::CrcFrameFormat::B,checked)
    ==wmbus_pure::CompleteStatus::Ok);
  assert(checked.bytes==encrypted);

  xmq_portable::CalcValue values[4]{};
  xmq_portable::CalendarDate dates[2]{};
  xmq_portable::DecodedPayload result{values,4,0,dates,2,0};
  using Status=xmq_portable::batch2_secure::Status;
  const auto decode=[&](const std::vector<uint8_t> &f,
                        const char *meter,const char *dll,
                        const uint8_t *aes) {
    return xmq_portable::batch2_secure::decode_configured(
        f.data(),f.size(),meter,"mkradio4a",aes,result,nullptr,dll);
  };
  assert(decode(checked.bytes,"66953825","00464936",key)==Status::Ok);
  assert(result.value_count==1 && result.date_count==1);
  assert(std::strcmp(result.values[0].name,"target_m3")==0);
  assert(std::fabs(result.values[0].value-16.1)<1e-9);
  assert(result.dates[0].year==2021 &&
         result.dates[0].month==12 && result.dates[0].day==31);
  // Original public fixture #2: cold water WEH/FE/07 behind TCH/95/37.
  // Application 75.2 m3 and 2021-12-31 independently encrypted with
  // the same published synthetic AES key (NOT actual upstream key).
  const auto cold=hex(
    "4B4468508644710095377202007701A85CFE078A003005"
    "4E98493C8CF7670F54B9D81B75D6AC2209D129DDEA873CEEA1B8669B20"
    "623F499F8DB95C1E3CE72E9B13E223764E230E2F2F2F2F2F");
  assert(cold.size()==76 && cold[0]==75);
  auto cold_air=cold;
  cold_air[0]=static_cast<uint8_t>(cold_air.size()+1);
  const auto cold_crc=wmbus_pure::crc16_en13757(
      cold_air.data(),cold_air.size());
  cold_air.push_back(static_cast<uint8_t>(cold_crc>>8));
  cold_air.push_back(static_cast<uint8_t>(cold_crc));
  wmbus_pure::CompleteFrame checked_cold{};
  assert(wmbus_pure::verify_complete_frame(
      cold_air.data(),cold_air.size(),wmbus_pure::CrcFrameFormat::B,
      checked_cold)==wmbus_pure::CompleteStatus::Ok);
  assert(decode(checked_cold.bytes,"01770002","00714486",key)==Status::Ok);
  assert(result.value_count==1 && result.date_count==1);
  assert(std::fabs(result.values[0].value-75.2)<1e-9);
  assert(result.dates[0].year==2021 &&
         result.dates[0].month==12 && result.dates[0].day==31);
  assert(decode(checked_cold.bytes,"01770003","00714486",key)!=Status::Ok);
  assert(result.value_count==0 && result.date_count==0);
  assert(decode(checked_cold.bytes,"01770002","00714487",key)!=Status::Ok);
  assert(result.value_count==0 && result.date_count==0);

  assert(decode(checked.bytes,"66953826","00464936",key)!=Status::Ok);
  assert(result.value_count==0 && result.date_count==0);
  assert(decode(checked.bytes,"66953825","00464937",key)==Status::WrongMeter);
  assert(result.value_count==0 && result.date_count==0);
  assert(decode(checked.bytes,"66953825",nullptr,key)!=Status::Ok);
  assert(result.value_count==0 && result.date_count==0);
  uint8_t wrong_key[16]{};std::memcpy(wrong_key,key,16);
  wrong_key[0]^=0x80;
  assert(decode(checked.bytes,"66953825","00464936",wrong_key)!=Status::Ok);
  assert(result.value_count==0 && result.date_count==0);
  assert(decode(checked.bytes,"66953825","00464936",nullptr)!=Status::Ok);
  auto modified=checked.bytes;
  modified[17]=0x01; // wrong nested HYD version
  assert(decode(modified,"66953825","00464936",key)!=Status::Ok);
  modified=checked.bytes;modified[15]^=1; // wrong TPL manufacturer
  assert(decode(modified,"66953825","00464936",key)!=Status::Ok);
  modified=checked.bytes;modified[22]=0; // security mode removed
  assert(decode(modified,"66953825","00464936",key)!=Status::Ok);
  modified=checked.bytes;modified[23]^=0x40; // corrupt CBC marker
  assert(decode(modified,"66953825","00464936",key)!=Status::Ok);
  modified=checked.bytes;modified.resize(71);modified[0]=70;
  assert(decode(modified,"66953825","00464936",key)!=Status::Ok);
  modified=checked.bytes;modified.push_back(0);modified[0]=76;
  assert(decode(modified,"66953825","00464936",key)!=Status::Ok);
  // Original source NOKEY is an already-decrypted application,
  // explicitly NOT a verifiable on-air encrypted frame.
  const auto supplied_plain=hex(
    "4B44685036494600953772253895662423FE064E003005"
    "2F2F4315A10000426CBF2C0F"
    "542CF2DD8BEC869511B2DB8301C3ABA390FB4FDB6F1144DA1F3897"
    "DD55F2AD0D194F68510FF8FADFB9");
  assert(supplied_plain.size()==76);
  assert(decode(supplied_plain,"66953825","00464936",key)!=Status::Ok);
  assert(result.value_count==0 && result.date_count==0);
  auto bad_crc=air;bad_crc[23]^=1;
  assert(wmbus_pure::verify_complete_frame(
    bad_crc.data(),bad_crc.size(),wmbus_pure::CrcFrameFormat::B,checked)
    !=wmbus_pure::CompleteStatus::Ok);
  // Unconfigured AUTO or the generic clear path MUST NOT decrypt.
  assert(xmq_portable::batch2_secure::decode_configured(
    encrypted.data(),encrypted.size(),"66953825","auto",
    key,result,nullptr,"00464936")!=Status::Ok);
  std::cout<<"PASS MK Radio4a configured long-TPL AES Mode5: both original target/date pairs;"
           <<" rejected wrong IDs/key, encrypted NOKEY, CRC, mode, model,"
           <<" truncation; no actual RF hardware validation.\n";
}
