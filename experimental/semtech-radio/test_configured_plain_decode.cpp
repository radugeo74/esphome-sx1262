#include "components/wmbus_native_sx126x/xmq_decode_catalog.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <vector>
static std::vector<uint8_t> hex(const char *s) {
  std::vector<uint8_t> out; int hi=-1;
  for(;*s;++s) {
    if(*s=='_') continue;
    int n=(*s>='0'&&*s<='9')?*s-'0':(*s>='A'&&*s<='F')?*s-'A'+10:-1;
    assert(n>=0);
    if(hi<0) hi=n;
    else {out.push_back(static_cast<uint8_t>((hi<<4)|n));hi=-1;}
  }
  assert(hi<0);return out;
}
static double get(const xmq_portable::DecodedPayload &out,const char *name) {
  for(size_t i=0;i<out.value_count;i++)
    if(std::strcmp(out.values[i].name,name)==0) return out.values[i].value;
  assert(false && "official field missing"); return 0;
}
int main() {
  using namespace xmq_portable;
  CalcValue values[24]{};
  CalendarDate dates[2]{};
  Capture captures[12]{};
  DecodedPayload result{values,24,0,dates,2,0};
  // Unmodified upstream iPERL T1 fixtures: explicit and resolved AUTO
  // ultimately enter this same configured-meter boundary.
  auto iperl=hex("1844AE4C4455223368077A55000000_041389E20100023B0000");
  assert(decode_configured_plain_frame(iperl.data(),iperl.size(),"33225544",
      "iperl",captures,12,result)==PureDecodeStatus::Ok);
  assert(std::fabs(get(result,"total_m3")-123.529)<1e-6);
  assert(get(result,"max_flow_m3h")==0);
  assert(decode_configured_plain_frame(iperl.data(),iperl.size(),"33225545",
      "iperl",captures,12,result)==PureDecodeStatus::InvalidMeterId);
  assert(result.value_count==0);
  // The public IZAR RS 868 fixture has an 0x8B DLL identification
  // octet, not a valid BCD digit. The original driver extracts the
  // secondary id in another way, but our strict RF boundary cannot
  // match this DLL as a configured 8-digit BCD ID. Its application
  // records remain valid input to the common already-clear decoder.
  auto izar=hex("1E4424238B07797389607A8F00107D_041312170100426CBF23441344100100");
  assert(decode_configured_plain_frame(izar.data(),izar.size(),"60897379",
      "hydrus",captures,12,result)==PureDecodeStatus::InvalidFrame);
  assert(result.value_count==0);
  size_t plain_count=0;
  assert(hydrus::decode_clear_application(izar.data()+15,izar.size()-15,
      values,24,plain_count)==hydrus::Status::Ok);
  result.value_count=plain_count;
  assert(std::fabs(get(result,"total_m3")-71.442)<1e-6);
  assert(std::fabs(get(result,"total_at_date_m3")-69.7)<1e-6);
  result.value_count=0;
  // Official Hydrus Mode-5 NOKEY fixture represents already-clear
  // post-decryption data, not plaintext RF. Fail closed at RF boundary.
  auto mode5=hex("4E44A5116464646470077AED004005_2F2F01FD08300C13741100007C1300000000FC101300000000FC201300000000726C00000B3B00000002FD748713025A6800C4016D3B177F2ACC011300020000");
  assert(decode_configured_plain_frame(mode5.data(),mode5.size(),"64646464",
      "hydrus",captures,12,result)==PureDecodeStatus::DecodeFailure);
  assert(result.value_count==0);
  // Complete configured-frame path using public fixture data with a
  // synthetic mode-zero TPL flag. The original mode-five frame above
  // must remain rejected: no decryption is attempted.
  mode5[14]=0x00;
  assert(decode_configured_plain_frame(mode5.data(),mode5.size(),"64646464",
      "hydrus",captures,12,result)==PureDecodeStatus::Ok);
  assert(std::fabs(get(result,"total_m3")-1.174)<1e-6);
  return 0;
}
