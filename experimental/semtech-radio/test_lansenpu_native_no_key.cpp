// Full original upstream Lansen PulseCounter 3x NOKEY fixtures through
// the EXACT native configured no-key dispatcher, status text adapter and
// synthetic DLL-format-B CRC verification. Does NOT test physical RF.
#include "components/wmbus_native_sx126x/xmq_batch2_plain_dispatch.h"
#include "components/wmbus_native_sx126x/xmq_plain_text_fields.h"
#include "components/wmbus_native_sx126x/pure_complete_frame.h"
#include "components/wmbus_native_sx126x/xmq_auto_registry.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>

static std::vector<uint8_t> unhex(const char *hex) {
  std::vector<uint8_t> out;int half=-1;
  while(*hex) {
    const char c=*hex++;
    if(c=='|' || c=='_') continue; // upstream application separators
    const int x=c>='0'&&c<='9'?c-'0':
                c>='a'&&c<='f'?c-'a'+10:
                c>='A'&&c<='F'?c-'A'+10:-1;
    assert(x>=0);
    if(half<0)half=x;
    else {out.push_back(static_cast<uint8_t>(half*16+x));half=-1;}
  }
  assert(half==-1);
  return out;
}
static bool value(const xmq_portable::DecodedPayload &p,
                  const char *name,double expected) {
  for(size_t i=0;i<p.value_count;++i)
    if(p.values[i].name && std::strcmp(p.values[i].name,name)==0)
      return std::fabs(p.values[i].value-expected)<1e-9;
  return false;
}
static std::vector<uint8_t> verify_crc(std::vector<uint8_t> clear) {
  // Public XMQ source fixtures are DLL CRC-stripped. Independently
  // construct an on-air format-B CRC and verify the real native gate.
  assert(clear.size()==static_cast<size_t>(clear[0])+1);
  std::vector<uint8_t> on_air=clear;
  on_air[0]=static_cast<uint8_t>(on_air.size()+1);
  const uint16_t crc=wmbus_pure::crc16_en13757(on_air.data(),on_air.size());
  on_air.push_back(static_cast<uint8_t>(crc>>8));
  on_air.push_back(static_cast<uint8_t>(crc));
  wmbus_pure::CompleteFrame checked{};
  assert(wmbus_pure::verify_complete_frame(
      on_air.data(),on_air.size(),wmbus_pure::CrcFrameFormat::B,checked)
      ==wmbus_pure::CompleteStatus::Ok);
  assert(checked.bytes==clear);
  on_air.back()^=1;
  assert(wmbus_pure::verify_complete_frame(
      on_air.data(),on_air.size(),wmbus_pure::CrcFrameFormat::B,checked)
      !=wmbus_pure::CompleteStatus::Ok);
  return clear;
}
int main() {
  const auto first=verify_crc(unhex(
    "234433300602010014007a8e0400002f2f"
    "0efd3a1147000000008e40fd3a341200000000"));
  const auto second=verify_crc(unhex(
    "1A443330503702000B027AD7000020|2F2F8E40FD3A700800000000"));
  const auto third=verify_crc(unhex(
    "1A443330503702000B027AD74C0020|2F2F8E40FD3A700800000000"));
  assert(first.size()==36 && second.size()==27 && third.size()==27);
  // Same selector as live T1/C1: configured-only AUTO Lansen,
  // metadata alone cannot override a bad ID or ambiguous model.
  const auto selected=[&](const std::vector<uint8_t> &raw,
                          const char *id,const char *requested="auto") {
    return xmq_portable::batch2_plain::select_configured_driver(
        raw.data(),raw.size(),id,id,requested,
        wmbus_pure::generated::signatures,
        wmbus_pure::generated::signature_count);
  };
  for(const auto *fixture:{&first,&second,&third}) {
    const char *id=(fixture==&first) ? "00010206" : "00023750";
    assert(selected(*fixture,id)!=nullptr);
    assert(std::strcmp(selected(*fixture,id),"lansenpu")==0);
  }
  assert(selected(first,"00010207")==nullptr);
  assert(xmq_portable::batch2_plain::select_configured_driver(
      first.data(),first.size(),"00010207","00010206","auto",
      wmbus_pure::generated::signatures,
      wmbus_pure::generated::signature_count)==nullptr);
  constexpr wmbus_pure::DriverSignature collision[]={
    {0x3033,0x14,0x00,"lansenpu"},
    {0x3033,0x14,0x00,"another_driver"}};
  assert(xmq_portable::batch2_plain::select_configured_driver(
      first.data(),first.size(),"00010206","00010206","auto",
      collision,2)==nullptr);

  xmq_portable::CalcValue values[4]{};
  xmq_portable::CalendarDate dates[1]{};
  xmq_portable::Capture captures[12]{};
  xmq_portable::DecodedPayload decoded{values,4,0,dates,1,0};
  using Status=xmq_portable::PureDecodeStatus;
  const auto decode=[&](const std::vector<uint8_t> &bytes,const char *id,
                        const char *driver="lansenpu") {
    return xmq_portable::batch2_plain::decode_configured(
        bytes.data(),bytes.size(),id,id,driver,captures,12,decoded);
  };
  assert(decode(first,"00010206")==Status::Ok);
  assert(decoded.value_count==2);
  assert(value(decoded,"a_counter",4711));
  assert(value(decoded,"b_counter",1234));
  auto status=xmq_portable::text_fields::decode(
      first.data(),first.size(),"lansenpu");
  assert(status.count==1 && std::strcmp(status.fields[0].name,"status")==0);
  assert(std::strcmp(status.fields[0].text,"POWER_LOW")==0);

  assert(decode(second,"00023750")==Status::Ok);
  assert(decoded.value_count==1 && value(decoded,"b_counter",870));
  status=xmq_portable::text_fields::decode(
      second.data(),second.size(),"lansenpu");
  assert(status.count==1 && std::strcmp(status.fields[0].text,"OK")==0);

  assert(decode(third,"00023750")==Status::Ok);
  assert(decoded.value_count==1 && value(decoded,"b_counter",870));
  status=xmq_portable::text_fields::decode(
      third.data(),third.size(),"lansenpu");
  assert(status.count==1);
  assert(std::strcmp(status.fields[0].text,
    "PERMANENT_ERROR POWER_LOW SABOTAGE_ENCLOSURE")==0);

  // A-only telegram: never fabricate a nonexistent B channel.
  auto a_only=first;
  a_only.resize(26);
  a_only[0]=static_cast<uint8_t>(a_only.size()-1);
  assert(decode(a_only,"00010206")==Status::Ok);
  assert(decoded.value_count==1 && value(decoded,"a_counter",4711));
  assert(decode(first,"00010207")==Status::InvalidMeterId);
  // Encryption may still match AUTO metadata; strict decoded-data admission
  // must reject it without stale counts or RSSI publication.
  auto auto_cipher=first;auto_cipher[14]=0x05;
  assert(selected(auto_cipher,"00010206")!=nullptr);
  assert(decode(auto_cipher,"00010206")!=Status::Ok);
  assert(decoded.value_count==0);
  assert(decoded.value_count==0);
  assert(decode(first,"00010206","auto")!=Status::Ok);
  assert(decoded.value_count==0);

  auto encrypted=first;
  encrypted[14]=0x05; // mode-5 AES (TPL configuration high byte)
  assert(decode(encrypted,"00010206")!=Status::Ok);
  assert(decoded.value_count==0);
  assert(xmq_portable::text_fields::decode(
      encrypted.data(),encrypted.size(),"lansenpu").count==0);

  auto invalid=first;
  invalid[20]=0xFA; // 12-digit BCD field contains invalid nibble
  assert(decode(invalid,"00010206")!=Status::Ok);
  assert(decoded.value_count==0);
  invalid=first;
  invalid[18]=0xFC; // VIF must match 0E FD 3A exactly
  assert(decode(invalid,"00010206")!=Status::Ok);
  assert(decoded.value_count==0);
  invalid=first;
  invalid[12]=0x80; // unknown TPL status must not be guessed
  assert(decode(invalid,"00010206")!=Status::Ok);
  assert(decoded.value_count==0);
  invalid=first;
  invalid[8]=0x15; // unknown Lansen model
  assert(decode(invalid,"00010206")!=Status::Ok);
  assert(decoded.value_count==0);
  invalid=first;
  invalid.insert(invalid.end(),first.begin()+26,first.end());
  invalid[0]=static_cast<uint8_t>(invalid.size()-1);
  assert(decode(invalid,"00010206")!=Status::Ok); // duplicate B field
  assert(decoded.value_count==0);
  invalid=first;
  invalid.pop_back();
  invalid[0]=static_cast<uint8_t>(invalid.size()-1);
  assert(decode(invalid,"00010206")!=Status::Ok); // truncated BCD
  assert(decoded.value_count==0);
  invalid=first;
  invalid[0]++; // malformed DLL length
  assert(decode(invalid,"00010206")!=Status::Ok);
  assert(decoded.value_count==0);
  std::cout<<"PASS Lansen native keyless: 3 upstream fixtures, "
              "A/B/status, wrong ID, encrypted, malformed, stale-value rejection; "
              "CRC envelope verified; not physical RF.\n";
  return 0;
}
