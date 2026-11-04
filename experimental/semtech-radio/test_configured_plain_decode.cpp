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
  // Existing working Techem decoders: the same configured-meter dispatch
  // must remain stable while the standard DIF/VIF drivers are integrated.
  auto mkradio4=hex("2F446850200141029562A2_06702901006017030004000300000000000000000000000000000000000000000000000000");
  assert(decode_configured_plain_frame(mkradio4.data(),mkradio4.size(),
      "02410120","mkradio4",captures,12,result)==PureDecodeStatus::Ok);
  assert(std::fabs(get(result,"total")-0.4)<1e-6);
  assert(std::fabs(get(result,"target")-0.1)<1e-6);
  // Official output names and prior internal XMQ names both remain usable.
  assert(std::fabs(get(result,"total_m3")-0.4)<1e-6);
  assert(std::fabs(get(result,"target_m3")-0.1)<1e-6);
  auto heat=hex("34446850226677116980A0119F27020480048300C408F709143C003D341A2B0B2A0707000000000000062D114457563D71A1850000");
  assert(decode_configured_plain_frame(heat.data(),heat.size(),
      "11776622","fhkvdataiii",captures,12,result)==PureDecodeStatus::Ok);
  assert(std::fabs(get(result,"current_hca")-131)<1e-6);
  assert(std::fabs(get(result,"previous_hca")-1026)<1e-6);
  // Date formulas are produced by the same configured RF dispatch, not
  // inferred from raw bytes or replaced by numeric sensor fields.
  const auto *heat_descriptor=find_decode_descriptor("fhkvdataiii",0xA0);
  assert(heat_descriptor && result.date_count==2);
  bool current_date=false, previous_date=false;
  for(size_t i=0;i<result.date_count;i++) {
    const auto &date=result.dates[i];
    const char *name=heat_descriptor->tables.date_calculations[i].field;
    if(std::strcmp(name,"current")==0) {
      assert(date.year==2020 && date.month==2 && date.day==8);
      current_date=true;
    } else if(std::strcmp(name,"previous")==0) {
      assert(date.year==2019 && date.month==12 && date.day==31);
      previous_date=true;
    }
  }
  assert(current_date && previous_date);
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
  // MK Radio 4a official NOKEY post-decryption application examples.
  // This is deliberately NOT the raw configured RF decoder entrypoint.
  auto mk4a_warm=hex("4315A10000426CBF2C0F542CF2DD8BEC869511B2DB8301C3ABA390FB4FDB6F1144DA1F3897DD55F2AD0D194F68510FF8FADFB9");
  assert(decode_supplied_clear_mkradio4a_application(
      mk4a_warm.data(),mk4a_warm.size(),result)==DriverStatus::Ok);
  assert(std::fabs(get(result,"target_m3")-16.1)<1e-6);
  assert(result.date_count==1 && result.dates[0].year==2021 &&
         result.dates[0].month==12 && result.dates[0].day==31);
  auto mk4a_cold=hex("4315F00200426CBF2C0FEE456BF6F802216503E25EB73E9377D54F672681B76C469696E4C7BCCC9072CC79F712360FC3F57D85");
  assert(decode_supplied_clear_mkradio4a_application(
      mk4a_cold.data(),mk4a_cold.size(),result)==DriverStatus::Ok);
  assert(std::fabs(get(result,"target_m3")-75.2)<1e-6);
  assert(result.date_count==1 && result.dates[0].year==2021 &&
         result.dates[0].month==12 && result.dates[0].day==31);
  assert(decode_supplied_clear_mkradio4a_application(
      mk4a_cold.data(),1,result)!=DriverStatus::Ok);
  assert(result.value_count==0 && result.date_count==0);
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
