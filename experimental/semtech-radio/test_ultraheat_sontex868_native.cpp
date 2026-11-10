// Real original XMQ public, CRC-stripped fixtures for Ultraheat and
// Sontex868. Synthetic EN13757 DLL format-B CRC envelope exercises
// native full-frame validation. No real RF/HA delivery is claimed.
#include "components/wmbus_native_sx126x/xmq_batch2_plain_dispatch.h"
#include "components/wmbus_native_sx126x/xmq_auto_registry.h"
#include "components/wmbus_native_sx126x/xmq_plain_text_fields.h"
#include "components/wmbus_native_sx126x/pure_complete_frame.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>

static std::vector<uint8_t> hex(const char *s) {
  std::vector<uint8_t> out;int half=-1;
  for(;*s;++s) {
    if(*s=='_' || *s=='|' || *s==' ' || *s=='\n')continue;
    const int d=(*s>='0'&&*s<='9')?*s-'0':
                (*s>='a'&&*s<='f')?*s-'a'+10:
                (*s>='A'&&*s<='F')?*s-'A'+10:-1;
    assert(d>=0);
    if(half<0)half=d;
    else {out.push_back(static_cast<uint8_t>(half*16+d));half=-1;}
  }
  assert(half<0);
  return out;
}
static void crc_checked(const std::vector<uint8_t> &f) {
  assert(f.size()==static_cast<size_t>(f[0])+1);
  // EN13757 format B: >126 DLL bytes use TWO CRC blocks.
  // The first covers the first 126 bytes, then two CRC bytes, then
  // the remaining payload followed by a second CRC. Never treat a
  // long frame as one checksum block.
  std::vector<uint8_t> air{};
  air.reserve(f.size()+4);
  if(f.size()<=126) {
    air=f;
    air[0]=static_cast<uint8_t>(f.size()+1);
    const auto crc=wmbus_pure::crc16_en13757(air.data(),air.size());
    air.push_back(static_cast<uint8_t>(crc>>8));
    air.push_back(static_cast<uint8_t>(crc));
  } else {
    air.assign(f.begin(),f.begin()+126);
    air[0]=static_cast<uint8_t>(f.size()+3);
    const auto first=wmbus_pure::crc16_en13757(air.data(),126);
    air.push_back(static_cast<uint8_t>(first>>8));
    air.push_back(static_cast<uint8_t>(first));
    const auto end=air.size();
    air.insert(air.end(),f.begin()+126,f.end());
    const auto second=wmbus_pure::crc16_en13757(
        air.data()+end,air.size()-end);
    air.push_back(static_cast<uint8_t>(second>>8));
    air.push_back(static_cast<uint8_t>(second));
  }
  wmbus_pure::CompleteFrame checked{};
  assert(wmbus_pure::verify_complete_frame(
      air.data(),air.size(),wmbus_pure::CrcFrameFormat::B,checked)
      ==wmbus_pure::CompleteStatus::Ok);
  assert(checked.bytes==f);
  air.back()^=1;
  assert(wmbus_pure::verify_complete_frame(
      air.data(),air.size(),wmbus_pure::CrcFrameFormat::B,checked)
      !=wmbus_pure::CompleteStatus::Ok);
}
static bool value(const xmq_portable::DecodedPayload &p,
                  const char *name,double expected,double delta=1e-5) {
  for(size_t i=0;i<p.value_count;++i)
    if(p.values[i].name && std::strcmp(p.values[i].name,name)==0)
      return std::fabs(p.values[i].value-expected)<delta;
  return false;
}
static const char *auto_driver(const std::vector<uint8_t> &f,
                               const char *id) {
  return xmq_portable::batch2_plain::select_configured_driver(
      f.data(),f.size(),id,id,"auto",
      wmbus_pure::generated::signatures,
      wmbus_pure::generated::signature_count);
}
int main() {
  // Original ultraheat.xmq #2: clear 7A, LUG/04/04, signed BCD power.
  const auto ultra=hex(
    "3b44a7320556637104047afa2000202f2f0c06774202000c1439995600"
    "0b2d0200f00b3b3018000a5a51030a5e520302fd170000066d0c080af42500");
  // Original sontex868.xmq (single public NOKEY 7A fixture).
  const auto sontex=hex(
    "AF46EE4D2827282716087A80000000_046D040A9F2A036E770000426CE1F7436"
    "E660000525900008288016C61258388016E0000008D8801EE1E3533FE0000000"
    "0000000000000000000000000000000000000000000000000000000000000000"
    "000000000000000000000000000000005FF2D0000803F8520FF2D0000803F025"
    "9AD0A0265D8041259AD0A8310FD3100000082106C01018110FD610082206C9F2"
    "A0BFD0F01030102FF2C000002FD66AC08");
  crc_checked(ultra);
  crc_checked(sontex);
  assert(auto_driver(ultra,"71635605") &&
         std::strcmp(auto_driver(ultra,"71635605"),"ultraheat")==0);
  assert(auto_driver(sontex,"27282728") &&
         std::strcmp(auto_driver(sontex,"27282728"),"sontex868")==0);
  assert(auto_driver(ultra,"71635606")==nullptr);
  assert(auto_driver(sontex,"27282729")==nullptr);
  xmq_portable::CalcValue vals[24]{};
  xmq_portable::CalendarDate dates[2]{};
  xmq_portable::Capture captures[12]{};
  xmq_portable::DecodedPayload out{vals,24,0,dates,2,0};
  using Status=xmq_portable::PureDecodeStatus;
  const auto decode=[&](const std::vector<uint8_t> &frame,
                        const char *id,const char *driver) {
    return xmq_portable::batch2_plain::decode_configured(
        frame.data(),frame.size(),id,id,driver,captures,12,out);
  };
  assert(decode(ultra,"71635605","ultraheat")==Status::Ok);
  assert(out.value_count==16);
  assert(value(out,"heat_kwh",24277));
  assert(value(out,"volume_m3",5699.39));
  assert(value(out,"power_kw",-0.2));
  assert(value(out,"flow_m3h",1.83));
  assert(value(out,"flow_c",35.1));
  assert(value(out,"return_c",35.2));
  assert(value(out,"tpl_status_raw",32));
  assert(value(out,"tpl_status_unknown_raw",32));
  for(const char *field:{"error_flags_raw","error_flags_unknown_raw",
       "status_alarm","status_busy","status_error","status_permanent_error",
       "status_power_low","status_temporary_error"})
    assert(value(out,field,0));
  const auto ultra_text=xmq_portable::text_fields::decode(
      ultra.data(),ultra.size(),"ultraheat");
  assert(ultra_text.count==2);
  assert(std::strcmp(ultra_text.fields[0].name,"status")==0);
  assert(std::strcmp(ultra_text.fields[0].text,"TPL_STS_32")==0);
  assert(std::strcmp(ultra_text.fields[1].name,"meter_datetime")==0);
  assert(std::strcmp(ultra_text.fields[1].text,"2023-05-20 10:08:12")==0);
  assert(decode(sontex,"27282728","sontex868")==Status::Ok);
  assert(value(out,"current_consumption_hca",119));
  assert(value(out,"consumption_at_set_date_hca",102));
  assert(value(out,"current_temp_c",27.33));
  assert(value(out,"current_room_temp_c",12.4));
  assert(value(out,"max_temp_c",27.33));
  assert(value(out,"max_temp_previous_period_c",0));
  // Native C? records are exposed with explicit storage/tariff labels.
  assert(out.value_count==13);
  assert(value(out,"storage_48_consumption_hca",0));
  assert(value(out,"tariff_1_duration_minutes",0));
  assert(value(out,"tariff_1_cumulation_counter",0));
  assert(value(out,"parameter_activation_state_raw",2220));
  assert(value(out,"manufacturer_ff2d_value",1));
  assert(value(out,"manufacturer_ff2d_tariff_2_value",1));
  assert(value(out,"manufacturer_ff2c_raw",0));
  const auto sontex_text=xmq_portable::text_fields::decode(
      sontex.data(),sontex.size(),"sontex868");
  const auto text_eq=[&](const char *field,const char *expected) {
    for(size_t i=0;i<sontex_text.count;++i)
      if(std::strcmp(sontex_text.fields[i].name,field)==0 &&
         std::strcmp(sontex_text.fields[i].text,expected)==0)return true;
    return false;
  };
  assert(sontex_text.count==8);
  assert(text_eq("set_date","2127-07-01"));
  assert(text_eq("device_datetime","2020-10-31 10:04"));
  assert(text_eq("storage_48_date","2019-05-01"));
  assert(text_eq("tariff_1_date","2000-01-01"));
  assert(text_eq("tariff_2_date","2020-10-31"));
  assert(text_eq("software_version","010301"));
  assert(text_eq("status","OK"));
  bool raw_profile=false;
  for(size_t i=0;i<sontex_text.count;++i)
    if(std::strcmp(sontex_text.fields[i].name,"compact_profile_hex")==0) {
      const char *hex=sontex_text.fields[i].text;
      raw_profile=std::strlen(hex)==106 && std::strncmp(hex,"33FE",4)==0;
    }
  assert(raw_profile);
  // Model-only matching never overrides original DLL meter ID or security.
  assert(decode(ultra,"71635606","ultraheat")!=Status::Ok &&
         out.value_count==0);
  assert(decode(sontex,"27282729","sontex868")!=Status::Ok &&
         out.value_count==0);
  auto encrypted=ultra;encrypted[14]=0x05;
  assert(auto_driver(encrypted,"71635605")!=nullptr); // metadata only
  assert(decode(encrypted,"71635605","ultraheat")!=Status::Ok &&
         out.value_count==0);
  encrypted=sontex;encrypted[14]=0x05;
  assert(decode(encrypted,"27282728","sontex868")!=Status::Ok &&
         out.value_count==0);
  auto tampered=sontex;tampered[57]=0x34; // raw compact length changed
  assert(decode(tampered,"27282728","sontex868")!=Status::Ok &&
         out.value_count==0);
  tampered=sontex;tampered[163]=0xFA; // non-BCD software version
  assert(decode(tampered,"27282728","sontex868")!=Status::Ok &&
         out.value_count==0);
  assert(xmq_portable::text_fields::decode(
      tampered.data(),tampered.size(),"sontex868").count==0);
  tampered=sontex;tampered[28]=0; // set_date: day must be nonzero
  assert(decode(tampered,"27282728","sontex868")!=Status::Ok &&
         out.value_count==0);
  auto invalid=ultra;invalid[8]=0x05; // different model
  assert(decode(invalid,"71635605","ultraheat")!=Status::Ok &&
         out.value_count==0);
  invalid=ultra;invalid.pop_back();invalid[0]=static_cast<uint8_t>(invalid.size()-1);
  assert(decode(invalid,"71635605","ultraheat")!=Status::Ok &&
         out.value_count==0);
  invalid=ultra;invalid[19]=0xFA; // malformed BCD nibble
  assert(decode(invalid,"71635605","ultraheat")!=Status::Ok &&
         out.value_count==0);
  invalid=ultra;invalid.insert(invalid.end(),ultra.begin()+17,ultra.begin()+23);
  invalid[0]=static_cast<uint8_t>(invalid.size()-1);
  assert(decode(invalid,"71635605","ultraheat")!=Status::Ok &&
         out.value_count==0);
  invalid=ultra;invalid[12]=0x00; // don't invent status OK variant
  assert(decode(invalid,"71635605","ultraheat")!=Status::Ok &&
         out.value_count==0);
  assert(xmq_portable::text_fields::decode(
      invalid.data(),invalid.size(),"ultraheat").count==0);
  invalid=ultra;invalid[12]=0x24; // unreviewed POWER_LOW bit
  assert(decode(invalid,"71635605","ultraheat")!=Status::Ok &&
         out.value_count==0);
  invalid=ultra;invalid[50]=0x01; // original 02FD17 error flags now nonzero
  assert(decode(invalid,"71635605","ultraheat")!=Status::Ok &&
         out.value_count==0);
  invalid=ultra;invalid[54]=0x7F; // 06 6D seconds outside 0..59
  assert(decode(invalid,"71635605","ultraheat")!=Status::Ok &&
         out.value_count==0);
  invalid=ultra;invalid[58]=0x00; // invalid calendar month
  assert(decode(invalid,"71635605","ultraheat")!=Status::Ok &&
         out.value_count==0);
  invalid=ultra;invalid[59]=0x01; // unsupported sixth date-time byte
  assert(decode(invalid,"71635605","ultraheat")!=Status::Ok &&
         out.value_count==0);
  xmq_portable::CalcValue limited_vals[15]{};
  xmq_portable::DecodedPayload limited{limited_vals,15,0,dates,2,0};
  assert(xmq_portable::batch2_plain::decode_configured(
      ultra.data(),ultra.size(),"71635605","71635605","ultraheat",
      captures,12,limited)==Status::DecodeFailure);
  assert(limited.value_count==0);
  // Original Ultraheat #3 is ELL + post-decrypted payload: do NOT
  // interpret its NOKEY label as a plaintext RF authorization.
  const auto ell=hex(
    "6344A7327856907204048C202B900F002C2592F604003504D66D5BEDC857"
    "7A2B004007102F2F_0C06690900000C14162800000B2D0000000B3B000000"
    "0A5A49070A5E19054C06000000008C01065100000002FD170000066D113814"
    "5B370002742F012F2F");
  assert(decode(ell,"72905678","ultraheat")!=Status::Ok &&
         out.value_count==0);
  // A standard Techem packet never masquerades as Sontex or Ultraheat.
  assert(decode(ultra,"71635605","sontex868")!=Status::Ok &&
         out.value_count==0);
  std::cout<<"PASS strict native Ultraheat/Sontex clear AUTO: public fixtures,"
            <<" six fields each, CRC envelope, bad ID/model/BCD/length,"
            <<" ciphertext, no ELL fallback; no physical RF.\n";
  return 0;
}
