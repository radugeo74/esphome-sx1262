// End-to-end RF -> DLL CRC -> admission -> configured XMQ decode.
// Uses only upstream/public driver fixtures already covered by CI.
#include "components/wmbus_native_sx126x/native_verified_frame.h"
#include "components/wmbus_native_sx126x/native_publication_gate.h"
#include "components/wmbus_native_sx126x/xmq_decode_catalog.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
namespace native=esphome::wmbus_native_sx126x;
static std::vector<uint8_t> from_hex(const char *s) {
  std::vector<uint8_t> out;
  int first=-1;
  while (*s) {
    const char c=*s++;
    if (c=='_') continue;
    const int n=c>='0'&&c<='9'?c-'0':c>='A'&&c<='F'?c-'A'+10:-1;
    assert(n>=0);
    if (first<0) first=n;
    else {out.push_back(static_cast<uint8_t>(first*16+n));first=-1;}
  }
  assert(first<0);
  return out;
}
static std::vector<uint8_t> format_a(const std::vector<uint8_t> &plain) {
  assert(!plain.empty() && plain.size()==size_t(plain[0])+1);
  std::vector<uint8_t> result;
  size_t at=0;
  while (at<plain.size()) {
    const size_t n=at==0?10:
        (plain.size()-at>16?16:plain.size()-at);
    assert(plain.size()-at>=n);
    const uint16_t crc=wmbus_pure::crc16_en13757(plain.data()+at,n);
    result.insert(result.end(),plain.begin()+at,plain.begin()+at+n);
    result.push_back(static_cast<uint8_t>(crc>>8));
    result.push_back(static_cast<uint8_t>(crc));
    at+=n;
  }
  return result;
}
static std::vector<uint8_t> t1_encode(const std::vector<uint8_t> &dll) {
  constexpr uint8_t codes[16]={0x16,0x0D,0x0E,0x0B,0x1C,0x19,0x1A,0x13,
                                0x2C,0x25,0x26,0x23,0x34,0x31,0x32,0x29};
  std::vector<uint8_t> encoded((dll.size()*12+7)/8,0);
  size_t bit=0;
  for (uint8_t b : dll) {
    for (uint8_t nibble : {static_cast<uint8_t>(b>>4),
                           static_cast<uint8_t>(b&0x0F)}) {
      for (int i=5;i>=0;--i,++bit)
        encoded[bit/8]|=static_cast<uint8_t>(((codes[nibble]>>i)&1u)<<(7-bit%8));
    }
  }
  return encoded;
}
static double field(const xmq_portable::DecodedPayload &out,const char *name) {
  for (size_t i=0;i<out.value_count;++i)
    if (out.values[i].name && std::strcmp(out.values[i].name,name)==0)
      return out.values[i].value;
  assert(false && "missing decoded fixture field");
  return 0;
}
static void run(const std::vector<uint8_t> &plain, const char *meter_id,
                const char *driver, const char *key, double expected) {
  const auto rf=format_a(plain);
  const auto t1=t1_encode(rf);
  const auto t=native::verified::process(native::boundary::Mode::T1,
                                         t1.data(),t1.size());
  const auto c=native::verified::process(native::boundary::Mode::C1A,
                                         rf.data(),rf.size());
  assert(t.status==native::verified::Status::Ok);
  assert(c.status==native::verified::Status::Ok);
  assert(t.frame.bytes==plain && c.frame.bytes==plain);
  assert(t.frame.header.meter_id==c.frame.header.meter_id);
  const uint32_t id=t.frame.header.meter_id;
  for (const auto *received : {&t,&c}) {
    assert(wmbus_pure::publication::inspect(received->frame.bytes.data(),
           received->frame.bytes.size(),id)==
           wmbus_pure::publication::Decision::Allowed);
    xmq_portable::CalcValue values[24]{};
    xmq_portable::CalendarDate dates[2]{};
    xmq_portable::Capture captures[12]{};
    xmq_portable::DecodedPayload decoded{values,24,0,dates,2,0};
    assert(xmq_portable::decode_configured_plain_frame(
           received->frame.bytes.data(),received->frame.bytes.size(),
           meter_id,driver,captures,12,decoded)==
           xmq_portable::PureDecodeStatus::Ok);
    assert(std::fabs(field(decoded,key)-expected)<1e-6);
    assert(wmbus_pure::publication::inspect(received->frame.bytes.data(),
           received->frame.bytes.size(),id+1)!=
           wmbus_pure::publication::Decision::Allowed);
  }
  auto damaged=rf;
  damaged[damaged.size()/2]^=0x01;
  const auto bad_c=native::verified::process(native::boundary::Mode::C1A,
                                             damaged.data(),damaged.size());
  assert(bad_c.status!=native::verified::Status::Ok && bad_c.frame.bytes.empty());
  const auto bad_t=t1_encode(damaged);
  const auto bad_t_result=native::verified::process(native::boundary::Mode::T1,
                                                   bad_t.data(),bad_t.size());
  assert(bad_t_result.status!=native::verified::Status::Ok &&
         bad_t_result.frame.bytes.empty());
}
int main() {
  const auto water=from_hex(
    "2F446850200141029562A2_06702901006017030004000300000000000000000000000000000000000000000000000000");
  run(water,"02410120","mkradio4","total_m3",0.4);
  const auto heat=from_hex(
    "34446850226677116980A0119F27020480048300C408F709143C003D341A2B0B2A0707000000000000062D114457563D71A1850000");
  run(heat,"11776622","fhkvdataiii","current_hca",131);
  return 0;
}
