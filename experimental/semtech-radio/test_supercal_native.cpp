// Original public Sontex Supercal 739 XMQ fixture. Source: wmbusmeters
// drivers/src/supercal.xmq (GPL-3.0-or-later).
// This test checks already CRC-stripped plaintext application only; it
// does not claim captured RF/HA validation or encrypted profile support.
#include "components/wmbus_native_sx126x/xmq_batch2_plain_dispatch.h"
#include "components/wmbus_native_sx126x/xmq_auto_registry.h"
#include "components/wmbus_native_sx126x/xmq_plain_text_fields.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>
static std::vector<uint8_t> hex(const char *str) {
  std::vector<uint8_t> out;
  int high=-1;
  for (;*str;++str) {
    if (*str=='_' || *str==' ' || *str=='\n') continue;
    const int d=(*str>='0' && *str<='9')?*str-'0':
                (*str>='A' && *str<='F')?*str-'A'+10:
                (*str>='a' && *str<='f')?*str-'a'+10:-1;
    assert(d>=0);
    if(high<0)high=d;
    else {out.push_back(static_cast<uint8_t>(high*16+d)); high=-1;}
  }
  assert(high<0);return out;
}
static bool value(const xmq_portable::DecodedPayload &p,
                  const char *field,double expected) {
  for(size_t i=0;i<p.value_count;++i)
    if(p.values[i].name && std::strcmp(field,p.values[i].name)==0)
      return std::fabs(p.values[i].value-expected)<1e-5;
  return false;
}
int main() {
  const auto original=hex(
    "4444EE4D198050891B047AFB000000_046D3612E423820A6CE1F1040628010000"
    "840A060000000004144A110000840A14000000000259E30F025D2A0C023B1D01032C3A0100");
  assert(original.size()==static_cast<size_t>(original[0])+1);
  const char *chosen=xmq_portable::batch2_plain::select_configured_driver(
      original.data(),original.size(),"89508019","89508019","auto",
      wmbus_pure::generated::signatures,
      wmbus_pure::generated::signature_count);
  assert(chosen && std::strcmp(chosen,"supercal")==0);
  xmq_portable::CalcValue fields[24]{};
  xmq_portable::CalendarDate dates[2]{};
  xmq_portable::Capture captures[12]{};
  xmq_portable::DecodedPayload out{fields,24,0,dates,2,0};
  using S=xmq_portable::PureDecodeStatus;
  const auto decode=[&](const std::vector<uint8_t> &f,const char *id,
                        const char *driver){
    return xmq_portable::batch2_plain::decode_configured(
        f.data(),f.size(),id,id,driver,captures,12,out);
  };
  assert(decode(original,"89508019","supercal")==S::Ok);
  assert(value(out,"total_kwh",296));
  assert(value(out,"total_m3",44.26));
  assert(value(out,"power_kw",3.14));
  assert(value(out,"volume_flow_m3h",0.285));
  assert(value(out,"flow_temperature_c",40.67));
  assert(value(out,"return_temperature_c",31.14));
  assert(value(out,"target_kwh",0));
  assert(value(out,"target_m3",0));
  assert(out.value_count==8);
  const auto text=xmq_portable::text_fields::decode(
      original.data(),original.size(),"supercal");
  const auto text_is=[&](const char *name,const char *value) {
    for(size_t i=0;i<text.count;++i)
      if(std::strcmp(text.fields[i].name,name)==0 &&
         std::strcmp(text.fields[i].text,value)==0)return true;
    return false;
  };
  assert(text.count==3);
  assert(text_is("target_date","2127-01-01"));
  assert(text_is("meter_datetime","2023-03-04 18:54"));
  assert(text_is("status","OK"));
  auto invalid=original;
  assert(decode(invalid,"89508018","supercal")!=S::Ok && out.value_count==0);
  invalid=original;invalid[8]=0x16;
  assert(decode(invalid,"89508019","supercal")!=S::Ok && out.value_count==0);
  invalid=original;invalid[14]=0x05;
  assert(decode(invalid,"89508019","supercal")!=S::Ok && out.value_count==0);
  invalid=original;invalid.pop_back();
  invalid[0]=static_cast<uint8_t>(invalid.size()-1);
  assert(decode(invalid,"89508019","supercal")!=S::Ok && out.value_count==0);
  invalid=original;invalid[0]^=1;
  assert(decode(invalid,"89508019","supercal")!=S::Ok && out.value_count==0);
  invalid=original;invalid[36]=0xFF; // malformed storage-20 energy data
  assert(decode(invalid,"89508019","supercal")!=S::Ok && out.value_count==0);
  assert(xmq_portable::text_fields::decode(
      invalid.data(),invalid.size(),"supercal").count==0);
  invalid=original;invalid[24]=0; // target date has no day
  assert(decode(invalid,"89508019","supercal")!=S::Ok && out.value_count==0);
  invalid=original;invalid[32]=0x04; // missing storage-20 energy marker
  assert(decode(invalid,"89508019","supercal")!=S::Ok && out.value_count==0);
  invalid=original;invalid[12]=0x08; // unvalidated TPL alarm
  assert(decode(invalid,"89508019","supercal")!=S::Ok && out.value_count==0);
  invalid=original;invalid[41]=0xFF; // total volume remains valid, target must not exceed total
  assert(decode(invalid,"89508019","supercal")!=S::Ok && out.value_count==0);
  assert(decode(original,"89508019","q400")!=S::Ok && out.value_count==0);
  std::cout<<"PASS Supercal configured clear native XMQ subset and negative gates; no RF/HA.\\n";
}
