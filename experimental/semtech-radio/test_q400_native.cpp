// Original wmbusmeters q400.xmq: 3 genuinely clear, 2 Mode-5
// post-decrypted examples. Only mode-zero RF application is accepted.
#include "components/wmbus_native_sx126x/xmq_batch2_plain_dispatch.h"
#include "components/wmbus_native_sx126x/xmq_auto_registry.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>
static std::vector<uint8_t> hex(const char *s) {
  std::vector<uint8_t> r;int high=-1;
  for(;*s;++s) {
    if(*s=='_' || *s==' ' || *s=='\n')continue;
    int d=(*s>='0'&&*s<='9')?*s-'0':
          (*s>='a'&&*s<='f')?*s-'a'+10:
          (*s>='A'&&*s<='F')?*s-'A'+10:-1;
    assert(d>=0);
    if(high<0)high=d;
    else {r.push_back(static_cast<uint8_t>(high*16+d));high=-1;}
  }
  assert(high<0);return r;
}
static bool value(const xmq_portable::DecodedPayload &p,const char *name,
                  double expected) {
  for(size_t i=0;i<p.value_count;++i)
    if(p.values[i].name && std::strcmp(p.values[i].name,name)==0)
      return std::fabs(p.values[i].value-expected)<0.00001;
  return false;
}
int main(){
  const char *source[]={
    "2E4409077272727210077AD71020052F2F_046D040D742C041377000000446D0000612C4413000000002F2F2F2F2F2F",
    "5E4409077372727210077A710050052F2F_046D0110A92704130022000004933B0000000004933C00000000023B000002592A0A446D0000A12744130000000044933B0000000044933C0000000001FD74622F2F2F2F2F2F2F2F2F2F2F2F2F2F",
    "544409076391820510077ABF100000046D2A0DC62C0420E80F430104130000000004933B0000000004933C00000000023B00000259F0D8446D0000C12C44130000000044933B0000000044933C0000000001FD7461",
    "8C4409071019221020077A56100000046D3008423804209081A20004130000000004933B0000000004933C00000000023B00000259F0D8446D0000413844130000000044933B0000000044933C00000000C401130000000034FD17010000000424407DA20001FD74630F0200000000000000000000000041380000000000000000000000000000000000000000",
    "614409074897280601077AD1000000046D2C173B370420580193040413FB94000004933BFB94000004933C01000000023B02000259E306446D000021374413C08A000044933BC08A000044933C0100000034FD17000000000424BA01930401FD745B"
  };
  std::vector<uint8_t> sample[5]{};
  for(size_t i=0;i<5;++i) {
    sample[i]=hex(source[i]);
    assert(sample[i].size()==static_cast<size_t>(sample[i][0])+1);
  }
  const char *ids[]={"72727272","72727273","05829163","10221910","06289748"};
  xmq_portable::CalcValue vals[24]{};
  xmq_portable::CalendarDate dates[2]{};
  xmq_portable::Capture cap[12]{};
  xmq_portable::DecodedPayload out{vals,24,0,dates,2,0};
  const auto decode=[&](size_t i,const char *id,const char *driver) {
    return xmq_portable::batch2_plain::decode_configured(
        sample[i].data(),sample[i].size(),id,id,driver,cap,12,out);
  };
  using S=xmq_portable::PureDecodeStatus;
  for(size_t i=2;i<5;++i) {
    const char *driver=xmq_portable::batch2_plain::select_configured_driver(
        sample[i].data(),sample[i].size(),ids[i],ids[i],"auto",
        wmbus_pure::generated::signatures,
        wmbus_pure::generated::signature_count);
    assert(driver && std::strcmp(driver,"q400")==0);
    const auto status=decode(i,ids[i],"q400");
    if(status!=S::Ok) std::cerr<<"Q400 source case "<<i+1<<" decode status "<<static_cast<int>(status)<<"\\n";
    assert(status==S::Ok);
  }
  assert(decode(2,ids[2],"q400")==S::Ok);
  assert(value(out,"total_m3",0));
  assert(value(out,"battery_pct",97));
  assert(value(out,"flow_temperature_c",-100));
  assert(decode(3,ids[3],"q400")==S::Ok);
  assert(value(out,"total_m3",0));
  assert(value(out,"battery_pct",99));
  assert(decode(4,ids[4],"q400")==S::Ok);
  assert(value(out,"total_m3",38.139));
  assert(value(out,"volume_flow_m3h",0.002));
  assert(value(out,"flow_temperature_c",17.63));
  assert(value(out,"battery_pct",91));
  // These NOKEY fixture applications follow encrypted Mode-5 headers.
  // Matching the model or seemingly valid record bytes cannot publish.
  for(size_t i=0;i<2;++i)
    assert(decode(i,ids[i],"q400")!=S::Ok && out.value_count==0);
  assert(decode(4,"06289749","q400")!=S::Ok && out.value_count==0);
  auto corrupted=sample[4];sample[4][8]=0x88;
  assert(decode(4,ids[4],"q400")!=S::Ok && out.value_count==0);
  sample[4]=corrupted;sample[4][14]=0x05;
  assert(decode(4,ids[4],"q400")!=S::Ok && out.value_count==0);
  sample[4]=corrupted;sample[4].pop_back();
  sample[4][0]=static_cast<uint8_t>(sample[4].size()-1);
  assert(decode(4,ids[4],"q400")!=S::Ok && out.value_count==0);
  std::cout<<"PASS configured native clear Q400: 3 public plaintext, 2 Mode-5 rejected, ID/model/mode/length; no RF\\n";
}
