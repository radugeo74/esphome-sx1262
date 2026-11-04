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
  assert(std::fabs(value(fields,count,"flow_m3h")-0.0)<0.000001);
  assert(std::fabs(value(fields,count,"flow_temperature_c")-10.4)<0.000001);
  assert(std::fabs(value(fields,count,"remaining_battery_life_y")-13.686797)<0.000001);
  assert(std::fabs(value(fields,count,"target_m3")-0.2)<0.000001);
  const auto warm=from_hex("2F2F0C13503400000DFD110A383731303134423032410B3B00000002FD74DC15C4016D3B178D29CC0113313400002F2F");
  assert(decode_clear_application(warm.data(),warm.size(),fields,32,count)==Status::Ok);
  assert(std::fabs(value(fields,count,"total_m3")-3.45)<0.00001);
  assert(std::fabs(value(fields,count,"remaining_battery_life_y")-15.321328)<0.000001);
  // Public hydrus.xmq IZAR RS cold and warm fixtures; only application
  // data following the original '_' delimiter is supplied to this parser.
  const auto izar=from_hex("041312170100426CBF23441344100100");
  assert(decode_clear_application(izar.data(),izar.size(),fields,32,count)==Status::Ok);
  assert(std::fabs(value(fields,count,"total_m3")-71.442)<0.000001);
  assert(std::fabs(value(fields,count,"total_at_date_m3")-69.7)<0.000001);
  const auto izar_warm=from_hex("0413DDC00000426CBF23441382BB0000");
  assert(decode_clear_application(izar_warm.data(),izar_warm.size(),fields,32,count)==Status::Ok);
  assert(std::fabs(value(fields,count,"total_m3")-49.373)<0.000001);
  assert(std::fabs(value(fields,count,"total_at_date_m3")-48.002)<0.000001);
  // Public Hydrus 2.0 post-underscore payload: min/max flow, target
  // and both water temperatures must match official JSON values.
  const auto hydrus20=from_hex("0C13921920000B3B0000000A5A96010A6622020C933C0000000084046D3B375F388C041341102000AB043B0000009B043B72170004FD17000000002F2F2F");
  assert(decode_clear_application(hydrus20.data(),hydrus20.size(),fields,32,count)==Status::Ok);
  assert(std::fabs(value(fields,count,"total_m3")-201.992)<0.000001);
  assert(std::fabs(value(fields,count,"target_m3")-201.041)<0.000001);
  assert(std::fabs(value(fields,count,"flow_temperature_c")-19.6)<0.000001);
  assert(std::fabs(value(fields,count,"external_temperature_c")-22.2)<0.000001);
  assert(std::fabs(value(fields,count,"min_flow_m3h"))<0.000001);
  assert(std::fabs(value(fields,count,"max_flow_m3h")-1.772)<0.000001);
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
