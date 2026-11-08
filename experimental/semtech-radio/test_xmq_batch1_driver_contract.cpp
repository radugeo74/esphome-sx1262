// First incremental driver batch: official XMQ fixtures only.
// Tests the configured live cleartext path and post-decryption-only boundary.
// No private meter IDs, keys, or captured RF data.
#include "components/wmbus_native_sx126x/xmq_decode_catalog.h"
#include "components/wmbus_native_sx126x/native_publication_gate.h"
#include "components/wmbus_native_sx126x/xmq_auto_dispatch.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <vector>

static std::vector<uint8_t> hex(const char *s) {
  std::vector<uint8_t> out;
  int half=-1;
  for (; *s; ++s) {
    if (*s=='_') continue;
    int d=(*s>='0'&&*s<='9')?*s-'0':
          (*s>='A'&&*s<='F')?*s-'A'+10:-1;
    assert(d>=0);
    if (half<0) half=d;
    else { out.push_back(static_cast<uint8_t>((half<<4)|d)); half=-1; }
  }
  assert(half<0);
  return out;
}
static double value(const xmq_portable::DecodedPayload &out,const char *name) {
  for(size_t i=0;i<out.value_count;i++)
    if(out.values[i].name && std::strcmp(out.values[i].name,name)==0)
      return out.values[i].value;
  assert(false && "official field missing");
  return 0;
}
int main() {
  using namespace xmq_portable;
  CalcValue v[24]{};
  CalendarDate dates[2]{};
  Capture captures[12]{};
  DecodedPayload out{v,24,0,dates,2,0};

  // Official iperl.xmq plaintext and status fixtures.
  auto iperl=hex("1844AE4C4455223368077A55000000_041389E20100023B0000");
  assert(wmbus_pure::publication::inspect(iperl.data(),iperl.size(),33225544)
      ==wmbus_pure::publication::Decision::Allowed);
  assert(decode_configured_plain_frame(iperl.data(),iperl.size(),
      "33225544","iperl",captures,12,out)==PureDecodeStatus::Ok);
  assert(std::fabs(value(out,"total_m3")-123.529)<1e-6);
  assert(value(out,"max_flow_m3h")==0);
  auto iperl_error=iperl;
  iperl_error[12]=0x06; // original iperl.xmq POWER_LOW TPL status example
  assert(decode_configured_plain_frame(iperl_error.data(),iperl_error.size(),
      "33225544","iperl",captures,12,out)==PureDecodeStatus::Ok);
  assert(std::fabs(value(out,"total_m3")-123.529)<1e-6);
  auto iperl_encrypted=iperl;
  iperl_encrypted[14]=0x05; // synthetic mode-5 security mutation
  assert(wmbus_pure::publication::inspect(
      iperl_encrypted.data(),iperl_encrypted.size(),33225544)
      !=wmbus_pure::publication::Decision::Allowed);
  assert(decode_configured_plain_frame(iperl_encrypted.data(),iperl_encrypted.size(),
      "33225544","iperl",captures,12,out)!=PureDecodeStatus::Ok);
  assert(out.value_count==0 && out.date_count==0);

  // Official hydrus.xmq post-decryption fixture must fail closed on live RF.
  auto hydrus=hex("4E44A5116464646470077AED004005_2F2F01FD08300C13741100007C1300000000FC101300000000FC201300000000726C00000B3B00000002FD748713025A6800C4016D3B177F2ACC011300020000");
  assert(wmbus_pure::publication::inspect(hydrus.data(),hydrus.size(),64646464)
      !=wmbus_pure::publication::Decision::Allowed);
  assert(decode_configured_plain_frame(hydrus.data(),hydrus.size(),
      "64646464","hydrus",captures,12,out)!=PureDecodeStatus::Ok);
  assert(out.value_count==0 && out.date_count==0);
  // A synthetic security-mode zero variant tests the real plaintext path.
  hydrus[14]=0;
  // M=0 but original N=4 is an invalid security configuration.
  assert(wmbus_pure::publication::inspect(hydrus.data(),hydrus.size(),64646464)
      ==wmbus_pure::publication::Decision::UnverifiedSecurity);
  hydrus[13]=0; // valid synthetic clear CF M=N=0
  assert(wmbus_pure::publication::inspect(hydrus.data(),hydrus.size(),64646464)
      ==wmbus_pure::publication::Decision::Allowed);
  assert(decode_configured_plain_frame(hydrus.data(),hydrus.size(),
      "64646464","hydrus",captures,12,out)==PureDecodeStatus::Ok);
  assert(std::fabs(value(out,"total_m3")-1.174)<1e-6);
  assert(std::fabs(value(out,"target_m3")-0.2)<1e-6);

  // Official mkradio4a.xmq contains a trusted clear application following
  // an upstream underscore. This does NOT authorize raw encrypted C1 decode.
  auto mkradio4a=hex("4B44685036494600953772253895662423FE064E0030052F2F_4315A10000426CBF2C0F542CF2DD8BEC869511B2DB8301C3ABA390FB4FDB6F1144DA1F3897DD55F2AD0D194F68510FF8FADFB9");
  assert(wmbus_pure::publication::inspect(mkradio4a.data(),mkradio4a.size(),66953825)
      !=wmbus_pure::publication::Decision::Allowed);
  assert(decode_configured_plain_frame(mkradio4a.data(),mkradio4a.size(),
      "66953825","mkradio4a",captures,12,out)!=PureDecodeStatus::Ok);
  assert(out.value_count==0 && out.date_count==0);
  assert(decode_supplied_clear_mkradio4a_application(
      mkradio4a.data()+25,mkradio4a.size()-25,out)==DriverStatus::Ok);
  assert(std::fabs(value(out,"target_m3")-16.1)<1e-6);
  assert(out.date_count==1 && out.dates[0].year==2021 &&
         out.dates[0].month==12 && out.dates[0].day==31);

  // No fabricated live support for a recognized converter.
  const wmbus_pure::DriverSignature recognized[]={{0x5068,0x95,0x37,"mkradio4a"}};
  const auto selection=wmbus_pure::auto_driver::resolve(
      "auto",recognized,1,0x5068,0x95,0x37);
  assert(selection.status==wmbus_pure::auto_driver::Status::NotCompiled);
  return 0;
}
