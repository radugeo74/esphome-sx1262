#include "components/wmbus_native_sx126x/iperl_plain.h"
#include "components/wmbus_native_sx126x/xmq_batch2_plain_dispatch.h"
#include "components/wmbus_native_sx126x/xmq_plain_text_fields.h"
#include "components/wmbus_native_sx126x/xmq_auto_registry.h"
#include "components/wmbus_native_sx126x/pure_complete_frame.h"
#include <cassert>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <vector>
static std::vector<uint8_t> hex(const char *s) {
  std::vector<uint8_t> result;
  int high = -1;
  for (; *s; ++s) {
    if (*s == '_') continue;
    int v = (*s >= '0' && *s <= '9') ? *s - '0' :
            (*s >= 'A' && *s <= 'F') ? *s - 'A' + 10 : -1;
    assert(v >= 0);
    if (high < 0) high = v;
    else { result.push_back(static_cast<uint8_t>((high << 4) | v)); high = -1; }
  }
  assert(high == -1);
  return result;
}
int main() {
  using namespace xmq_portable::iperl;
  Reading r{};
  auto a=hex("1E44AE4C9956341268077A36001000_2F2F0413181E0000023B00002F2F2F2F");
  // Upstream #1 says "after decryption", but its CF is M=0,N=1.
  // The RF entrypoint must REJECT it; only offline clear-app parsing
  // may read its already-supplied measurement records.
  assert(decode(a.data(),a.size(),r) == Status::Encrypted);
  assert(r.total_m3==0.0 && r.max_flow_m3h==0.0);
  assert(decode_clear_application(a.data()+15,a.size()-15,r)==Status::Ok);
  assert(std::fabs(r.total_m3 - 7.704)<0.00001);
  assert(r.max_flow_m3h == 0.0 && !r.power_low);
  auto b=hex("1844AE4C4455223368077A55000000_041389E20100023B0000");
  assert(decode(b.data(),b.size(),r) == Status::Ok);
  assert(std::fabs(r.total_m3 - 123.529)<0.00001);
  assert(r.max_flow_m3h == 0.0 && !r.power_low);
  auto c=hex("1844AE4C4455223368077A55060000_041389E20100023B0000");
  assert(decode(c.data(),c.size(),r) == Status::Ok);
  assert(r.power_low && r.tpl_status == 0x06);
  // Exactly source-proven MVTs; no accepting SEN/7C/06 by combining
  // a valid version with a valid, but unrelated, medium.
  auto unproven=b;
  unproven[8]=0x7C;unproven[9]=0x06;
  assert(decode(unproven.data(),unproven.size(),r)==Status::Unsupported);
  assert(r.total_m3==0.0);
  // Unverified status bits are rejected rather than published as OK.
  unproven=b;unproven[12]=0x04;
  assert(decode(unproven.data(),unproven.size(),r)==Status::Unsupported);
  assert(r.total_m3==0.0);
  unproven=b;unproven[12]=0x80;
  assert(decode(unproven.data(),unproven.size(),r)==Status::Unsupported);
  assert(r.total_m3==0.0);

  // Real configured path: all numeric flags and text must match the
  // two source-proven Mode0 examples. AUTO requires exact MVT and ID.
  const auto auto_driver=[&](const std::vector<uint8_t>&frame,
                             const char*requested) {
    return wmbus_pure::auto_driver::resolve(
       requested,wmbus_pure::generated::signatures,
       wmbus_pure::generated::signature_count,
       0x4CAE,0x68,0x07);
  };
  const auto picked=auto_driver(b,"auto");
  assert(picked.status==wmbus_pure::auto_driver::Status::Ready);
  assert(picked.driver && std::strcmp(picked.driver,"iperl")==0);
  xmq_portable::CalcValue values[12]{};
  xmq_portable::CalendarDate dates[2]{};
  xmq_portable::Capture captures[12]{};
  xmq_portable::DecodedPayload result{values,12,0,dates,2,0};
  const auto configured=[&](const std::vector<uint8_t>&frame,
                            const char*id) {
    return xmq_portable::decode_configured_plain_frame(
      frame.data(),frame.size(),id,picked.driver,captures,12,result);
  };
  const auto has=[&](const char*field,double expected) {
    for(size_t i=0;i<result.value_count;++i)
      if(result.values[i].name &&
         std::strcmp(result.values[i].name,field)==0 &&
         std::fabs(result.values[i].value-expected)<0.00001)
        return true;
    return false;
  };
  const auto check_numeric=[&](const std::vector<uint8_t>&frame,
                                uint8_t status) {
    assert(configured(frame,"33225544")==xmq_portable::PureDecodeStatus::Ok);
    assert(result.value_count==10);
    assert(has("total_m3",123.529) && has("max_flow_m3h",0));
    assert(has("tpl_status_raw",status));
    assert(has("tpl_status_unknown_raw",0));
    assert(has("status_alarm",0) && has("status_busy",0));
    assert(has("status_error",status==0x06?1:0));
    assert(has("status_power_low",status==0x06?1:0));
    assert(has("status_permanent_error",0) &&
           has("status_temporary_error",0));
    const auto text=xmq_portable::text_fields::decode(
       frame.data(),frame.size(),"iperl");
    assert(text.count==1);
    assert(std::strcmp(text.fields[0].name,"status")==0);
    assert(std::strcmp(text.fields[0].text,
       status==0x06?"ERROR POWER_LOW":"OK")==0);
  };
  check_numeric(b,0x00);
  auto warning=hex("1844AE4C4455223368077A55060000_041389E20100023B0000");
  check_numeric(warning,0x06);
  assert(configured(b,"33225545")==xmq_portable::PureDecodeStatus::InvalidMeterId);
  assert(result.value_count==0);
  assert(configured(a,"12345699")!=xmq_portable::PureDecodeStatus::Ok);
  assert(result.value_count==0);
  unproven=b;unproven[12]=0x04;
  assert(configured(unproven,"33225544")!=xmq_portable::PureDecodeStatus::Ok);
  assert(result.value_count==0);
  assert(xmq_portable::text_fields::decode(
      unproven.data(),unproven.size(),"iperl").count==0);

  // A smaller mapping buffer may not lose the diagnostic fields silently.
  xmq_portable::CalcValue limited_vals[9]{};
  xmq_portable::DecodedPayload limited{limited_vals,9,0,dates,2,0};
  assert(xmq_portable::decode_configured_plain_frame(
      b.data(),b.size(),"33225544","iperl",captures,12,limited)
      ==xmq_portable::PureDecodeStatus::DecodeFailure);
  assert(limited.value_count==0);

  // Synthetic format-B on-air CRC envelope: the original XMQ source
  // fixtures have no attached DLL CRC and are NOT captured live RF.
  auto air=b;
  air[0]=static_cast<uint8_t>(air.size()+1);
  const uint16_t crc=wmbus_pure::crc16_en13757(air.data(),air.size());
  air.push_back(static_cast<uint8_t>(crc>>8));
  air.push_back(static_cast<uint8_t>(crc));
  wmbus_pure::CompleteFrame verified{};
  assert(wmbus_pure::verify_complete_frame(
      air.data(),air.size(),wmbus_pure::CrcFrameFormat::B,verified)
      ==wmbus_pure::CompleteStatus::Ok);
  assert(verified.bytes==b);
  air.back()^=1;
  assert(wmbus_pure::verify_complete_frame(
      air.data(),air.size(),wmbus_pure::CrcFrameFormat::B,verified)
      !=wmbus_pure::CompleteStatus::Ok);

  // Mutations: no reporting values after a failed frame.
  c[0]=0x19;
  assert(decode(c.data(),c.size(),r) == Status::InvalidFrame);
  assert(r.total_m3==0.0);
  c=b; c[13]=0x00; c[14]=0x05; // AES mode 5 must not be interpreted as plaintext
  assert(decode(c.data(),c.size(),r) == Status::Encrypted);
  c=b; c[13]=0x10; c[14]=0x00; // Mode 0 with N=1 is INVALID OMS
  assert(decode(c.data(),c.size(),r) == Status::Encrypted);
  assert(r.total_m3==0.0);
  c=b; c[13]=0x00; c[14]=0x10; // 5th mode bit: M=16, NOT M=0
  assert(decode(c.data(),c.size(),r) == Status::Encrypted);
  assert(r.total_m3==0.0);
  c=b; c[15]=0x84; // unsupported DIFE must fail closed
  assert(decode(c.data(),c.size(),r) == Status::Unsupported);
  c=b; c.resize(20); c[0]=19; // truncated records with consistent DLL length
  assert(decode(c.data(),c.size(),r) != Status::Ok);
  return 0;
}
