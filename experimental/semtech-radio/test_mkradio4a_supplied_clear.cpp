// MK Radio4a: both original NOKEY fixtures decode keylessly
// OFFLINE only. DLL converter and actual nested TPL meter IDs must match.
// This deliberately does not authorize Mode-5 live plaintext publication.
#include "experimental/semtech-radio/host_only/mkradio4a_supplied_clear_fixture.h"
#include "components/wmbus_native_sx126x/xmq_batch2_plain_dispatch.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>
static std::vector<uint8_t> unhex(const char *s){
  std::vector<uint8_t> out;int hi=-1;
  for(;*s;++s) {
    if(*s=='_' || *s==' ' || *s=='\n')continue;
    int d=(*s>='0' && *s<='9')?*s-'0':
          (*s>='A' && *s<='F')?*s-'A'+10:
          (*s>='a' && *s<='f')?*s-'a'+10:-1;
    assert(d>=0);
    if(hi<0)hi=d;
    else {out.push_back(static_cast<uint8_t>(hi*16+d));hi=-1;}
  }
  assert(hi<0);return out;
}
int main() {
  const char *source[]={
    "4B44685036494600953772253895662423FE064E0030052F2F_4315A10000426CBF2C0F542CF2DD8BEC869511B2DB8301C3ABA390FB4FDB6F1144DA1F3897DD55F2AD0D194F68510FF8FADFB9",
    "4B4468508644710095377202007701A85CFE078A0030052F2F_4315F00200426CBF2C0FEE456BF6F802216503E25EB73E9377D54F672681B76C469696E4C7BCCC9072CC79F712360FC3F57D85"
  };
  const char *meter[]={"66953825","01770002"};
  const char *dll[]={"00464936","00714486"};
  const double volume[]={16.1,75.2};
  using xmq_portable::mkradio4a::Reading;
  using S=xmq_portable::mkradio4a::supplied_clear::Status;
  using xmq_portable::mkradio4a::supplied_clear::decode;
  for(size_t i=0;i<2;++i){
    auto frame=unhex(source[i]);
    assert(frame.size()==76 && frame.size()==static_cast<size_t>(frame[0])+1);
    Reading reading{};
    assert(decode(frame.data(),frame.size(),meter[i],dll[i],reading)==S::Ok);
    assert(std::fabs(reading.target_m3-volume[i])<0.000001);
    assert(reading.year==2021 && reading.month==12 && reading.day==31);
    assert(decode(frame.data(),frame.size(),meter[i],"00000000",reading)==S::WrongDll);
    assert(reading.target_m3==0 && reading.year==0);
    assert(decode(frame.data(),frame.size(),"00000000",dll[i],reading)==S::WrongTpl);
    auto bad=frame;bad[8]=0x94;
    assert(decode(bad.data(),bad.size(),meter[i],dll[i],reading)==S::WrongModel);
    bad=frame;bad[21]=0;
    assert(decode(bad.data(),bad.size(),meter[i],dll[i],reading)==S::NotSuppliedClear);
    bad=frame;bad[23]=0;
    assert(decode(bad.data(),bad.size(),meter[i],dll[i],reading)==S::NotSuppliedClear);
    bad=frame;bad[25]=0x13;
    assert(decode(bad.data(),bad.size(),meter[i],dll[i],reading)!=S::Ok);
    bad=frame;bad[0]^=1;
    assert(decode(bad.data(),bad.size(),meter[i],dll[i],reading)==S::InvalidFrame);
    bad=frame;bad.pop_back();bad[0]=static_cast<uint8_t>(bad.size()-1);
    assert(decode(bad.data(),bad.size(),meter[i],dll[i],reading)!=S::Ok);
    xmq_portable::CalcValue nums[8]{};
    xmq_portable::CalendarDate dates[2]{};
    xmq_portable::Capture captures[8]{};
    xmq_portable::DecodedPayload out{nums,8,0,dates,2,0};
    assert(xmq_portable::batch2_plain::decode_configured(
        frame.data(),frame.size(),meter[i],dll[i],"mkradio4a",
        captures,8,out)!=xmq_portable::PureDecodeStatus::Ok);
    assert(out.value_count==0 && out.date_count==0);
  }
  std::cout<<"PASS MK Radio4a keyless upstream NOKEY warm/cold offline; "
              "strict DLL/TPL identity and negative tests; live RF blocked.\n";
}
