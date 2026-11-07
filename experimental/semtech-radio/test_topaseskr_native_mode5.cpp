// Topas secure receiver: original public wmbusmeters post-decrypted
// XMQ applications, independently encrypted using synthetic AES key.
// DOES NOT claim original ciphertext or physical RF/HA validation.
#include "components/wmbus_native_sx126x/xmq_batch2_secure_dispatch.h"
#include <mbedtls/aes.h>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>
static std::vector<uint8_t> hex(const char *p) {
  std::vector<uint8_t> bytes;int high=-1;
  for(;*p;++p) {
    if(*p=='_' || *p==' ' || *p=='\n')continue;
    const int digit=(*p>='0'&&*p<='9')?*p-'0':
      (*p>='a'&&*p<='f')?*p-'a'+10:
      (*p>='A'&&*p<='F')?*p-'A'+10:-1;
    assert(digit>=0);
    if(high<0) high=digit;
    else {bytes.push_back(static_cast<uint8_t>(high*16+digit));high=-1;}
  }
  assert(high<0);return bytes;
}
static std::vector<uint8_t> encrypt_synthetic(const std::vector<uint8_t> &clear,
                                              const uint8_t key[16]) {
  assert(clear.size()==79 && clear[15]==0x2F && clear[16]==0x2F);
  uint8_t iv[16]{};
  std::memcpy(iv,clear.data()+2,8);
  std::memset(iv+8,clear[11],8);
  std::vector<uint8_t> encrypted=clear;
  mbedtls_aes_context context;
  mbedtls_aes_init(&context);
  assert(mbedtls_aes_setkey_enc(&context,key,128)==0);
  assert(mbedtls_aes_crypt_cbc(&context,MBEDTLS_AES_ENCRYPT,64,
      iv,clear.data()+15,encrypted.data()+15)==0);
  mbedtls_aes_free(&context);
  return encrypted;
}
static bool value(const xmq_portable::DecodedPayload &p,
                  const char *name,double expected) {
  for(size_t i=0;i<p.value_count;++i)
    if(p.values[i].name && std::strcmp(name,p.values[i].name)==0)
      return std::fabs(p.values[i].value-expected)<0.00001;
  return false;
}
int main() {
  const char *original[]={
    "4E44B40512345678F1077A310040052F2F_01FD08040C13991848004C1359423500CC101300000000CC201359423500426C7F2C0B3B00000002FD74DA10025AD300C4016D3B179F27CC011387124600",
    "4E44B40553021969F1077A0C0040052F2F_01FD08800C13914544004C1393673500CC101300000000CC201393673500426CDF2C0B3B0100F002FD747912025AAE00C4016D3B17FF25CC011325584100"
  };
  const char *ids[]={"78563412","69190253"};
  const double expected_total[]={481.899,444.591};
  const double expected_temp[]={21.1,17.4};
  const double expected_flow[]={0.0,-0.001};
  const double expected_year[]={354.259,356.793};
  const uint8_t key[16]={1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
  uint8_t wrong[16]={1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,17};
  xmq_portable::CalcValue fields[24]{};
  xmq_portable::CalendarDate dates[2]{};
  xmq_portable::DecodedPayload result{fields,24,0,dates,2,0};
  using S=xmq_portable::batch2_secure::Status;
  for(size_t i=0;i<2;++i) {
    const auto clear=hex(original[i]);
    const auto encrypted=encrypt_synthetic(clear,key);
    const auto decode=[&](const std::vector<uint8_t> &frame,
                          const char *id,const uint8_t *secret) {
      return xmq_portable::batch2_secure::decode_configured(
          frame.data(),frame.size(),id,"topaseskr",secret,result);
    };
    assert(decode(encrypted,ids[i],key)==S::Ok);
    assert(value(result,"total_m3",expected_total[i]));
    assert(value(result,"temperature_c",expected_temp[i]));
    assert(value(result,"current_flow_m3h",expected_flow[i]));
    assert(value(result,"volume_year_period_m3",expected_year[i]));
    // NOKEY example is post-decrypted, never pass it as original RF.
    assert(decode(clear,ids[i],key)!=S::Ok && result.value_count==0);
    assert(decode(encrypted,ids[i],wrong)!=S::Ok && result.value_count==0);
    assert(decode(encrypted,i==0?"78563413":"69190254",key)!=S::Ok &&
           result.value_count==0);
    auto invalid=encrypted;invalid[8]=0x00;
    assert(decode(invalid,ids[i],key)!=S::Ok && result.value_count==0);
    invalid=encrypted;invalid[14]=0;
    assert(decode(invalid,ids[i],key)!=S::Ok && result.value_count==0);
    invalid=encrypted;invalid[15]^=1;
    assert(decode(invalid,ids[i],key)!=S::Ok && result.value_count==0);
    invalid=encrypted;invalid.pop_back();
    invalid[0]=static_cast<uint8_t>(invalid.size()-1);
    assert(decode(invalid,ids[i],key)!=S::Ok && result.value_count==0);
    assert(decode(encrypted,ids[i],nullptr)!=S::Ok && result.value_count==0);
  }
  std::cout<<"PASS Topas native explicit-key Mode5 synthetic CBC over 2 "
              "original applications and negative gates; not original RF.\\n";
}
