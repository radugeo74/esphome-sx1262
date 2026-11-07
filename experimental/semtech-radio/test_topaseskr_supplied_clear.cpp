// Decode both public original Topas NOKEY frames WITHOUT an AES key,
// as offline already-clear fixture data. Never a live RF acceptance test.
#include "experimental/semtech-radio/host_only/topaseskr_supplied_clear_fixture.h"
#include "components/wmbus_native_sx126x/xmq_batch2_plain_dispatch.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

static std::vector<uint8_t> unhex(const char *s) {
  std::vector<uint8_t> out;int hi=-1;
  for(;*s;++s) {
    if(*s=='_' || *s==' ' || *s=='\n') continue;
    const int c=(*s>='0'&&*s<='9')?*s-'0':
      (*s>='A'&&*s<='F')?*s-'A'+10:
      (*s>='a'&&*s<='f')?*s-'a'+10:-1;
    assert(c>=0);
    if(hi<0) hi=c;
    else {out.push_back(static_cast<uint8_t>(hi*16+c));hi=-1;}
  }
  assert(hi<0);return out;
}
int main() {
  const char *public_source[]={
    "4E44B40512345678F1077A310040052F2F_01FD08040C13991848004C1359423500CC101300000000CC201359423500426C7F2C0B3B00000002FD74DA10025AD300C4016D3B179F27CC011387124600",
    "4E44B40553021969F1077A0C0040052F2F_01FD08800C13914544004C1393673500CC101300000000CC201393673500426CDF2C0B3B0100F002FD747912025AAE00C4016D3B17FF25CC011325584100"
  };
  const char *ids[]={"78563412","69190253"};
  const double total[]={481.899,444.591},temp[]={21.1,17.4},
               flow[]={0,-0.001},year[]={354.259,356.793};
  using xmq_portable::topaseskr::Reading;
  using Offline=xmq_portable::topaseskr::supplied_clear::Status;
  using xmq_portable::topaseskr::supplied_clear::decode;
  for(size_t i=0;i<2;++i) {
    const auto original=unhex(public_source[i]);
    assert(original.size()==79 && original.size()==static_cast<size_t>(original[0])+1);
    Reading reading{};
    assert(decode(original.data(),original.size(),ids[i],reading)==Offline::Ok);
    assert(std::fabs(reading.total_m3-total[i])<1e-5);
    assert(std::fabs(reading.temperature_c-temp[i])<1e-5);
    assert(std::fabs(reading.flow_m3h-flow[i])<1e-5);
    assert(reading.year && std::fabs(reading.year_m3-year[i])<1e-5);
    assert(decode(original.data(),original.size(),"00000000",reading)==Offline::WrongMeter);
    assert(!reading.total && !reading.temperature && !reading.flow && !reading.year);
    auto invalid=original;invalid[8]=0x02;
    assert(decode(invalid.data(),invalid.size(),ids[i],reading)==Offline::WrongModel);
    invalid=original;invalid[14]=0;
    assert(decode(invalid.data(),invalid.size(),ids[i],reading)==Offline::NotSuppliedClear);
    invalid=original;invalid[15]=0x00;
    assert(decode(invalid.data(),invalid.size(),ids[i],reading)==Offline::NotSuppliedClear);
    invalid=original;invalid[17]=0xFF; // broken first DIF, no invented records
    assert(decode(invalid.data(),invalid.size(),ids[i],reading)!=Offline::Ok);
    invalid=original;invalid.pop_back();
    invalid[0]=static_cast<uint8_t>(invalid.size()-1);
    assert(decode(invalid.data(),invalid.size(),ids[i],reading)!=Offline::Ok);
    invalid=original;invalid[0]^=1;
    assert(decode(invalid.data(),invalid.size(),ids[i],reading)==Offline::InvalidFrame);
    // Crucial: the LIVE configured plaintext entrypoint must NEVER
    // accept Mode-5 NOKEY bytes or start publishing without a key.
    xmq_portable::CalcValue numbers[8]{};
    xmq_portable::CalendarDate dates[2]{};
    xmq_portable::Capture captures[8]{};
    xmq_portable::DecodedPayload out{numbers,8,0,dates,2,0};
    assert(xmq_portable::batch2_plain::decode_configured(
        original.data(),original.size(),ids[i],ids[i],"topaseskr",
        captures,8,out)!=xmq_portable::PureDecodeStatus::Ok);
    assert(out.value_count==0 && out.date_count==0);
  }
  std::cout<<"PASS original Topas NOKEY both fixtures keyless offline, negative guards, live Mode5 fail closed.\\n";
}
