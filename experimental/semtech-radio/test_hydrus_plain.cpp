#include "components/wmbus_native_sx126x/hydrus_plain.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <vector>
static std::vector<uint8_t> from_hex(const char *s) {
  std::vector<uint8_t> v; int h=-1;
  for (; *s; ++s) {
    if (*s=='_') continue;
    int n=(*s>='0'&&*s<='9')?*s-'0':
          (*s>='A'&&*s<='F')?*s-'A'+10:-1;
    assert(n>=0);
    if(h<0) h=n;
    else {v.push_back(static_cast<uint8_t>((h<<4)|n));h=-1;}
  }
  assert(h<0);
  return v;
}
static double value(const xmq_portable::CalcValue *v,size_t n,const char *name) {
  for(size_t i=0;i<n;i++) if(std::strcmp(v[i].name,name)==0) return v[i].value;
  assert(false);return 0;
}
int main() {
  using namespace xmq_portable::hydrus;
  xmq_portable::CalcValue fields[32]{};
  size_t count=0;
  // Original hydrus.xmq public fixture, application data after underscore:
  // upstream represents this as already available plaintext, not a decrypt test.
  const auto water=from_hex("2F2F01FD08300C13741100007C1300000000FC101300000000FC201300000000726C00000B3B00000002FD748713025A6800C4016D3B177F2ACC011300020000");
  const auto s=decode_clear_application(water.data(),water.size(),fields,32,count);
  assert(s==Status::Ok);
  assert(std::fabs(value(fields,count,"total_m3")-1.174)<0.00001);
  const auto warm=from_hex("2F2F0C13503400000DFD110A383731303134423032410B3B00000002FD74DC15C4016D3B178D29CC0113313400002F2F");
  assert(decode_clear_application(warm.data(),warm.size(),fields,32,count)==Status::Ok);
  assert(std::fabs(value(fields,count,"total_m3")-3.45)<0.00001);
  // Incomplete/wrong records fail closed, never leave previous readings.
  auto broken=from_hex("0C1350340000");
  assert(decode_clear_application(broken.data(),broken.size()-1,fields,32,count)!=Status::Ok);
  assert(count==0);
  const auto no_total=from_hex("023B0000");
  assert(decode_clear_application(no_total.data(),no_total.size(),fields,32,count)==Status::MissingFields);
  assert(count==0);
  // The upstream test includes a readable POST-DECRYPTION application
  // after '_' but the original TPL configuration 0x0540 indicates MODE 5.
  // Our RF receiver must NOT interpret this as an unencrypted telegram.
  auto raw=from_hex("4E44A5116464646470077AED004005_2F2F01FD08300C13741100007C1300000000FC101300000000FC201300000000726C00000B3B00000002FD748713025A6800C4016D3B177F2ACC011300020000");
  assert(decode_plain_frame(raw.data(),raw.size(),fields,32,count)==Status::Encrypted);
  assert(count==0);
  // Security mode 0 may be decoded only if payload is actually plaintext.
  raw[14]=0x00;
  assert(decode_plain_frame(raw.data(),raw.size(),fields,32,count)==Status::Ok);
  assert(std::fabs(value(fields,count,"total_m3")-1.174)<0.00001);
  return 0;
}
