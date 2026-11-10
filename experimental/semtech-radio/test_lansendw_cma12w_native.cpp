// Four original public wmbusmeters keyless fixtures for Lansen DW and
// Elvaco CMA12w. Native configured AUTO, strict clear decode and text
// field mapping. Original fixture bytes are independently CRC-stripped;
// synthetic DLL format-B CRC envelope exercises the actual frame verifier.
// NO actual physical RF or Home Assistant runtime is claimed.
#include "components/wmbus_native_sx126x/xmq_batch2_plain_dispatch.h"
#include "components/wmbus_native_sx126x/xmq_plain_text_fields.h"
#include "components/wmbus_native_sx126x/xmq_auto_registry.h"
#include "components/wmbus_native_sx126x/pure_complete_frame.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>

static std::vector<uint8_t> hex(const char *s) {
  std::vector<uint8_t> out;int pending=-1;
  while(*s) {
    const char c=*s++;
    if(c=='|' || c=='_' || c==' ' || c=='\n')continue;
    const int d=(c>='0'&&c<='9')?c-'0':
                (c>='a'&&c<='f')?c-'a'+10:
                (c>='A'&&c<='F')?c-'A'+10:-1;
    assert(d>=0);
    if(pending<0) pending=d;
    else {out.push_back(static_cast<uint8_t>(pending*16+d));pending=-1;}
  }
  assert(pending<0);return out;
}
static void verify_crc(const std::vector<uint8_t> &plain) {
  assert(plain.size()==static_cast<size_t>(plain[0])+1);
  std::vector<uint8_t> air=plain;
  air[0]=static_cast<uint8_t>(air.size()+1);
  const uint16_t crc=wmbus_pure::crc16_en13757(air.data(),air.size());
  air.push_back(static_cast<uint8_t>(crc>>8));
  air.push_back(static_cast<uint8_t>(crc));
  wmbus_pure::CompleteFrame checked{};
  assert(wmbus_pure::verify_complete_frame(
      air.data(),air.size(),wmbus_pure::CrcFrameFormat::B,checked)
      ==wmbus_pure::CompleteStatus::Ok);
  assert(checked.bytes==plain);
  air.back()^=1;
  assert(wmbus_pure::verify_complete_frame(
      air.data(),air.size(),wmbus_pure::CrcFrameFormat::B,checked)
      !=wmbus_pure::CompleteStatus::Ok);
}
static const char *select_auto(const std::vector<uint8_t> &f,
                               const char *id) {
  return xmq_portable::batch2_plain::select_configured_driver(
      f.data(),f.size(),id,id,"auto",
      wmbus_pure::generated::signatures,
      wmbus_pure::generated::signature_count);
}
static bool has_value(const xmq_portable::DecodedPayload &r,
                      const char *field,double expected) {
  for(size_t i=0;i<r.value_count;++i)
    if(r.values[i].name && std::strcmp(field,r.values[i].name)==0)
      return std::fabs(r.values[i].value-expected)<1e-7;
  return false;
}
static bool has_text(const xmq_portable::text_fields::Result &r,
                     const char *field,const char *expected) {
  for(size_t i=0;i<r.count;++i)
    if(r.fields[i].name && std::strcmp(r.fields[i].name,field)==0)
      return std::strcmp(r.fields[i].text,expected)==0;
  return false;
}
int main(){
  // All four are original XMQ complete DLL-CRC-stripped frames.
  const auto door_closed=hex(
    "2e44333005020100071d7ab54800002f2f02fd1b110002fd971d0100"
    "0efd3a2200000000008e40fd3a000000000000");
  const auto door_open=hex(
    "2e44333005020100071d7ab66800002f2f02fd1b550002fd971d0100"
    "0efd3a2300000000008e40fd3a000000000000");
  const auto room_1=hex(
    "2744961566666666201b7af9000020_2f2f02651e0942651809"
    "02fd1b30030dfd0f05302e302e340f");
  const auto room_2=hex(
    "27449615023900681f1b7ac5000020_2f2f02651e0942652109"
    "02fd1b30420dfd0f05352e302e330f");
  for(const auto *frame:{&door_closed,&door_open,&room_1,&room_2})
    verify_crc(*frame);

  xmq_portable::CalcValue values[24]{};
  xmq_portable::CalendarDate dates[2]{};
  xmq_portable::Capture captures[12]{};
  xmq_portable::DecodedPayload result{values,24,0,dates,2,0};
  using Status=xmq_portable::PureDecodeStatus;
  const auto decoded=[&](const std::vector<uint8_t> &f,const char *id,
                         const char *driver) {
    return xmq_portable::batch2_plain::decode_configured(
        f.data(),f.size(),id,id,driver,captures,12,result);
  };
  for(const auto *f:{&door_closed,&door_open}) {
    assert(select_auto(*f,"00010205")!=nullptr);
    assert(std::strcmp(select_auto(*f,"00010205"),"lansendw")==0);
    assert(decoded(*f,"00010205","lansendw")==Status::Ok);
    assert(result.value_count==14);
    assert(has_value(result,"b_counter",0));
    assert(has_value(result,"door_open",f==&door_open?1:0));
    assert(has_value(result,"digital_input_raw",f==&door_open?85:17));
    assert(has_value(result,"error_flags_raw",1));
    assert(has_value(result,"error_flags_unknown_raw",1));
    assert(has_value(result,"tpl_status_raw",f==&door_open?104:72));
    assert(has_value(result,"tpl_status_unknown_raw",f==&door_open?96:64));
    assert(has_value(result,"status_alarm",0));
    assert(has_value(result,"status_busy",0));
    assert(has_value(result,"status_error",0));
    assert(has_value(result,"status_permanent_error",1));
    assert(has_value(result,"status_power_low",0));
    assert(has_value(result,"status_temporary_error",0));
  }
  assert(decoded(door_closed,"00010205","lansendw")==Status::Ok);
  assert(has_value(result,"a_counter",22));
  auto text=xmq_portable::text_fields::decode(
      door_closed.data(),door_closed.size(),"lansendw");
  assert(has_text(text,"status","CLOSED"));
  assert(text.count==2);
  assert(has_text(text,"error_flags",
                  "ERROR_FLAGS_1 PERMANENT_ERROR TPL_STS_64"));
  assert(decoded(door_open,"00010205","lansendw")==Status::Ok);
  assert(has_value(result,"a_counter",23));
  text=xmq_portable::text_fields::decode(
      door_open.data(),door_open.size(),"lansendw");
  assert(has_text(text,"status","OPEN"));
  assert(text.count==2);
  assert(has_text(text,"error_flags",
                  "ERROR_FLAGS_1 PERMANENT_ERROR TPL_STS_96"));

  assert(select_auto(room_1,"66666666")!=nullptr);
  assert(std::strcmp(select_auto(room_1,"66666666"),"cma12w")==0);
  assert(decoded(room_1,"66666666","cma12w")==Status::Ok);
  assert(result.value_count==11);
  assert(has_value(result,"current_temperature_c",23.34));
  assert(has_value(result,"average_temperature_1h_c",23.28));
  assert(has_value(result,"battery_raw",0x0330));
  for(const char *field:{"tpl_status_raw","tpl_status_unknown_raw",
           "status_alarm","status_busy","status_error",
           "status_permanent_error","status_power_low","status_temporary_error"})
    assert(has_value(result,field,0));
  text=xmq_portable::text_fields::decode(
      room_1.data(),room_1.size(),"cma12w");
  assert(has_text(text,"status","OK"));
  assert(has_text(text,"battery","BATTERY_330"));
  assert(has_text(text,"software_version","4.0.0"));
  assert(text.count==3);
  assert(select_auto(room_2,"68003902")!=nullptr);
  assert(std::strcmp(select_auto(room_2,"68003902"),"cma12w")==0);
  assert(decoded(room_2,"68003902","cma12w")==Status::Ok);
  assert(result.value_count==11);
  assert(has_value(result,"current_temperature_c",23.34));
  assert(has_value(result,"average_temperature_1h_c",23.37));
  assert(has_value(result,"battery_raw",0x4230));
  for(const char *field:{"tpl_status_raw","tpl_status_unknown_raw",
           "status_alarm","status_busy","status_error",
           "status_permanent_error","status_power_low","status_temporary_error"})
    assert(has_value(result,field,0));
  text=xmq_portable::text_fields::decode(
      room_2.data(),room_2.size(),"cma12w");
  assert(has_text(text,"status","OK"));
  assert(has_text(text,"battery","BATTERY_4230"));
  assert(has_text(text,"software_version","3.0.5"));
  assert(text.count==3);

  // Unknown configured IDs and unknown signatures never resolve AUTO.
  assert(select_auto(door_closed,"00010206")==nullptr);
  assert(select_auto(room_1,"66666667")==nullptr);
  const auto select_with=[&](const std::vector<uint8_t>&f,const char *id,
                             const wmbus_pure::DriverSignature *s,size_t n) {
    return xmq_portable::batch2_plain::select_configured_driver(
        f.data(),f.size(),id,id,"auto",s,n);
  };
  const wmbus_pure::DriverSignature ambiguous[]={
    {0x3033,0x07,0x1D,"lansendw"},
    {0x3033,0x07,0x1D,"another"}};
  assert(select_with(door_closed,"00010205",ambiguous,2)==nullptr);

  // Encrypted Mode 5 may match MVT but must not decode or publish.
  auto bad=door_closed;bad[14]=0x05;
  assert(select_auto(bad,"00010205")!=nullptr);
  assert(decoded(bad,"00010205","lansendw")!=Status::Ok);
  assert(result.value_count==0);
  assert(xmq_portable::text_fields::decode(
      bad.data(),bad.size(),"lansendw").count==0);
  bad=room_1;bad[14]=0x25;
  assert(select_auto(bad,"66666666")!=nullptr);
  assert(decoded(bad,"66666666","cma12w")!=Status::Ok);
  assert(result.value_count==0);
  assert(xmq_portable::text_fields::decode(
      bad.data(),bad.size(),"cma12w").count==0);

  // Door: bad BCD, unknown digital input, invalid flags and duplicates.
  bad=door_closed;bad[32]=0xFA;
  assert(decoded(bad,"00010205","lansendw")!=Status::Ok);
  assert(result.value_count==0);
  bad=door_closed;bad[20]=0x22; // digital input not OPEN or CLOSED
  assert(decoded(bad,"00010205","lansendw")!=Status::Ok);
  bad=door_closed;bad[26]=0x02; // unknown error flags
  assert(decoded(bad,"00010205","lansendw")!=Status::Ok);
  assert(result.value_count==0);
  assert(xmq_portable::text_fields::decode(
      bad.data(),bad.size(),"lansendw").count==0);
  bad=door_open;bad[12]=0x69; // extra unverified TPL status bit
  assert(decoded(bad,"00010205","lansendw")!=Status::Ok);
  assert(result.value_count==0);
  assert(xmq_portable::text_fields::decode(
      bad.data(),bad.size(),"lansendw").count==0);
  bad=door_closed;bad[12]=0x68; // same raw OPEN TPL, but actual state CLOSED
  // The status byte alone is not used to infer OPEN/CLOSED.
  assert(decoded(bad,"00010205","lansendw")==Status::Ok);
  assert(has_value(result,"door_open",0));
  text=xmq_portable::text_fields::decode(
      bad.data(),bad.size(),"lansendw");
  assert(has_text(text,"status","CLOSED"));
  bad=door_closed;bad.insert(bad.end(),
                             door_closed.begin()+17,door_closed.begin()+22);
  bad[0]=static_cast<uint8_t>(bad.size()-1);
  assert(decoded(bad,"00010205","lansendw")!=Status::Ok);
  bad=door_closed;bad.pop_back();bad[0]=static_cast<uint8_t>(bad.size()-1);
  assert(decoded(bad,"00010205","lansendw")!=Status::Ok);

  // CMA: rejected TPL states cannot produce falsely healthy zero flags.
  bad=room_1;bad[12]=0x04;
  assert(decoded(bad,"66666666","cma12w")!=Status::Ok);
  assert(result.value_count==0);
  assert(xmq_portable::text_fields::decode(
      bad.data(),bad.size(),"cma12w").count==0);
  bad=room_2;bad[12]=0x80;
  assert(decoded(bad,"68003902","cma12w")!=Status::Ok);
  assert(result.value_count==0);
  assert(xmq_portable::text_fields::decode(
      bad.data(),bad.size(),"cma12w").count==0);
  // A capacity of 10 cannot hold the 11 optional numeric fields.
  xmq_portable::CalcValue limited_values[10]{};
  xmq_portable::DecodedPayload limited{limited_values,10,0,dates,2,0};
  assert(xmq_portable::batch2_plain::decode_configured(
      room_1.data(),room_1.size(),"66666666","66666666",
      "cma12w",captures,12,limited)==Status::DecodeFailure);
  assert(limited.value_count==0);

  // CMA: wrong model, bad record, missing software and malformed L.
  bad=room_1;bad[8]=0x21;
  assert(decoded(bad,"66666666","cma12w")!=Status::Ok);
  bad=room_1;bad[18]=0x64;
  assert(decoded(bad,"66666666","cma12w")!=Status::Ok);
  bad=room_1;bad[34]='X';
  assert(decoded(bad,"66666666","cma12w")!=Status::Ok);
  bad=room_1;bad.resize(28);bad[0]=static_cast<uint8_t>(bad.size()-1);
  assert(decoded(bad,"66666666","cma12w")!=Status::Ok);
  bad=room_1;bad[0]++;
  assert(decoded(bad,"66666666","cma12w")!=Status::Ok);
  assert(result.value_count==0);

  std::cout<<"PASS Lansen DW + Elvaco CMA12w: 4 original clear NOKEY "
              "fixtures, configured AUTO, CRC, text, negative security/IDs. "
              "No physical RF.\n";
  return 0;
}
