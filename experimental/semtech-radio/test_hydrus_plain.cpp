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
  assert(std::fabs(value(fields,count,"backward_m3"))<0.000001);
  // Typed date regressions from the same unchanged upstream Hydrus fixtures.
  DateField dates[4]{};
  size_t date_count=0;
  assert(decode_clear_application(izar.data(),izar.size(),fields,32,count,
                                  dates,4,&date_count)==Status::Ok);
  assert(date_count==1);
  assert(std::strcmp(dates[0].name,"at_date")==0);
  assert(dates[0].value.year==2021 && dates[0].value.month==3 && dates[0].value.day==31);
  assert(decode_clear_application(water.data(),water.size(),fields,32,count,
                                  dates,4,&date_count)==Status::Ok);
  assert(date_count==1);
  assert(std::strcmp(dates[0].name,"target_datetime")==0);
  assert(dates[0].value.year==2019 && dates[0].value.month==10 &&
         dates[0].value.day==31 && dates[0].value.hour==23 &&
         dates[0].value.minute==59);
  assert(decode_clear_application(hydrus20.data(),hydrus20.size(),fields,32,count,
                                  dates,4,&date_count)==Status::Ok);
  assert(date_count==1 && dates[0].value.year==2026 &&
         dates[0].value.month==8 && dates[0].value.day==31);
  // A chained VIFE can describe a different physical quantity and must not
  // inherit the simple remaining-battery field name from its first modifier.
  const auto compound_battery=from_hex("0C135034000002FDF402DC15");
  assert(decode_clear_application(compound_battery.data(),compound_battery.size(),
                                  fields,32,count)==Status::Ok);
  assert(count==1 && std::strcmp(fields[0].name,"total_m3")==0);
  // Even a zero-valued VIFE makes the volume a modified field, not a total.
  const auto modified_volume=from_hex("0C930050340000");
  assert(decode_clear_application(modified_volume.data(),modified_volume.size(),
                                  fields,32,count)==Status::MissingFields);
  assert(count==0);
  // Date with a combinable VIFE is not the ordinary billing date.
  // The primary total is valid, but the modified date stays unexposed.
  const auto modified_date=from_hex("04131217010042EC00BF23");
  assert(decode_clear_application(modified_date.data(),modified_date.size(),
                                  fields,32,count,dates,4,&date_count)==Status::Ok);
  assert(count==1 && std::strcmp(fields[0].name,"total_m3")==0);
  assert(date_count==0);
  // A volume from subunit 1 must not override subunit 0.
  const auto secondary_unit=from_hex("844013DDC00000041312170100");
  assert(decode_clear_application(secondary_unit.data(),secondary_unit.size(),
                                  fields,32,count)==Status::Ok);
  assert(count==1 && std::strcmp(fields[0].name,"total_m3")==0);
  assert(std::fabs(fields[0].value-71.442)<0.000001);
  // A tariff-1 flow, temperature or battery value must not become the
  // primary reading. Existing main volume is preserved.
  const auto tariffed=from_hex(
      "0C135034000082103BE80382105AC400821066DE00"
      "8210FD748713");
  assert(decode_clear_application(tariffed.data(),tariffed.size(),
                                  fields,32,count)==Status::Ok);
  assert(count==1 && std::strcmp(fields[0].name,"total_m3")==0);
  assert(std::fabs(fields[0].value-3.45)<0.000001);
  const auto tariff_backward=from_hex("0C13503400008210933CE803");
  assert(decode_clear_application(tariff_backward.data(),tariff_backward.size(),
                                  fields,32,count)==Status::Ok);
  assert(count==1 && std::strcmp(fields[0].name,"total_m3")==0);
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
  // Old fixture mutated only M, leaving N=4: invalid OMS Mode 0.
  assert(decode_plain_frame(raw.data(),raw.size(),fields,32,count)!=Status::Ok);
  raw[13]=0x00; // valid synthetic Mode 0 requires M=N=0
  assert(decode_plain_frame(raw.data(),raw.size(),fields,32,count)==Status::Ok);
  assert(std::fabs(value(fields,count,"total_m3")-1.174)<0.00001);
  return 0;
}
