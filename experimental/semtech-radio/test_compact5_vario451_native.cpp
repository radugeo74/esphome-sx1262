// Genuine upstream Techem CI-A2 original samples; no private IDs or keys.
// Negative cases reject CI A1, M/V/T drift, wrong IDs and bad layouts.
#include "components/wmbus_native_sx126x/xmq_batch2_plain_dispatch.h"
#include "components/wmbus_native_sx126x/xmq_auto_registry.h"
#include "components/wmbus_native_sx126x/native_publication_gate.h"
#include "components/wmbus_native_sx126x/xmq_plain_text_fields.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <vector>
#include <utility>
static std::vector<uint8_t> hex(const char*s) {
 std::vector<uint8_t> out;int hi=-1;
 for(;*s;++s) {
  if(*s=='_'||*s==' ')continue;
  int x=*s>='0'&&*s<='9'?*s-'0':*s>='a'&&*s<='f'?*s-'a'+10:
    *s>='A'&&*s<='F'?*s-'A'+10:-1;
  assert(x>=0);
  if(hi<0)hi=x;else{out.push_back(uint8_t((hi<<4)|x));hi=-1;}
 }
 assert(hi<0);return out;
}
static bool has(const xmq_portable::DecodedPayload&o,const char*n,double v) {
 for(size_t i=0;i<o.value_count;++i)
  if(o.values[i].name && std::strcmp(o.values[i].name,n)==0 &&
     std::fabs(o.values[i].value-v)<0.00001)return true;
 return false;
}
int main() {
 using namespace xmq_portable;
 const auto c0=hex("37446850336633663943a2_10672c866100181c01000480794435d50000000000000000000000000000000000000000000000000000000000");
 const auto c1=hex("37446850792055673943A2_109F2F13C500608F1D00008066E8A69B26988D335F6411450C564C5145145CA0F1DA35B9DD37A1936BBF3D31D8");
 const auto v0=hex("374468506549235827C3A2_129F25383300A8622600008200800A2AF862115175552877A36F26C9AB1CB24400000004000000000004908002");
 const auto a1=hex("36446850626262624543A1_009F2777010060780000000A000000000000000000000000000000000000000000000000A0400000B4010000");
 CalcValue values[16]{};CalendarDate dates[2]{};Capture caps[12]{};
 DecodedPayload out{values,16,0,dates,2,0};
 const auto auto_driver=[&](const std::vector<uint8_t>&f,const char*id){
  return batch2_plain::select_configured_driver(
    f.data(),f.size(),id,id,"auto",wmbus_pure::generated::signatures,
    wmbus_pure::generated::signature_count);
 };
 const auto dec=[&](const std::vector<uint8_t>&f,const char*id,const char*drv){
  return batch2_plain::decode_configured(f.data(),f.size(),id,id,drv,caps,12,out);
 };
 for(const auto &p:std::vector<std::pair<const std::vector<uint8_t>*,const char*>>{
    {&c0,"66336633"},{&c1,"67552079"}}) {
  const char *selected=auto_driver(*p.first,p.second);
  assert(selected && std::strcmp(selected,"compact5")==0);
  assert(dec(*p.first,p.second,"compact5")==PureDecodeStatus::Ok);
 }
 assert(dec(c0,"66336633","compact5")==PureDecodeStatus::Ok);
 assert(out.value_count==3 && has(out,"total_kwh",25250) &&
        has(out,"current_kwh",284) && has(out,"previous_kwh",24966));
 assert(dec(c1,"67552079","compact5")==PureDecodeStatus::Ok);
 assert(out.value_count==3 && has(out,"total_kwh",58018) &&
        has(out,"current_kwh",7567) && has(out,"previous_kwh",50451));
 const char *selected=auto_driver(v0,"58234965");
 assert(selected && std::strcmp(selected,"vario451")==0);
 assert(dec(v0,"58234965","vario451")==PureDecodeStatus::Ok);
 assert(out.value_count==5 && has(out,"total_kwh",6371.666667) &&
        has(out,"current_kwh",2729.444444) &&
        has(out,"previous_kwh",3642.222222) &&
        has(out,"prev_raw_counter",13112) &&
        has(out,"curr_raw_counter",9826));
 const auto vario_text=text_fields::decode(v0.data(),v0.size(),"vario451");
 assert(vario_text.count==1);
 assert(std::strcmp(vario_text.fields[0].name,"mfct_payload_hex")==0);
 assert(std::strlen(vario_text.fields[0].text)==90);
 assert(std::strcmp(vario_text.fields[0].text,
     "129F25383300A8622600008200800A2AF862115175552877A36F26C9AB1CB24400000004000000000004908002")==0);
 assert(wmbus_pure::publication::inspect(c0.data(),c0.size(),66336633)==
        wmbus_pure::publication::Decision::Allowed);
 assert(wmbus_pure::publication::inspect(a1.data(),a1.size(),62626262)!=
        wmbus_pure::publication::Decision::Allowed);
 assert(dec(a1,"62626262","compact5")!=PureDecodeStatus::Ok);
 assert(out.value_count==0);
 assert(dec(c0,"66336634","compact5")!=PureDecodeStatus::Ok);
 assert(out.value_count==0);
 auto bad=c0;bad[10]=0xA1;
 assert(dec(bad,"66336633","compact5")!=PureDecodeStatus::Ok);
 bad=c0;bad[12]=0x00;
 assert(dec(bad,"66336633","compact5")!=PureDecodeStatus::Ok);
 bad=c1;bad[17]=0x00;
 assert(dec(bad,"67552079","compact5")!=PureDecodeStatus::Ok);
 bad=c1;bad[0]++;
 assert(dec(bad,"67552079","compact5")!=PureDecodeStatus::Ok);
 bad=v0;bad[8]=0x28;
 assert(dec(bad,"58234965","vario451")!=PureDecodeStatus::Ok);
 // XMQ lists TCH/27/04 as metadata, but only TCH/27/C3 is radio-fixture
 // verified; changing to 27/04 must not be treated as valid plaintext.
 bad=v0;bad[9]=0x04;
 assert(dec(bad,"58234965","vario451")!=PureDecodeStatus::Ok);
 bad=v0;bad[11]=0x13;
 assert(dec(bad,"58234965","vario451")!=PureDecodeStatus::Ok);
 bad=v0;bad[13]=0x00;
 assert(dec(bad,"58234965","vario451")!=PureDecodeStatus::Ok);
 bad=v0;bad[16]=0xFF;
 assert(dec(bad,"58234965","vario451")!=PureDecodeStatus::Ok);
 bad=v0;bad[10]=0x7A;
 assert(dec(bad,"58234965","vario451")!=PureDecodeStatus::Ok);
 assert(out.value_count==0);
 assert(text_fields::decode(bad.data(),bad.size(),"vario451").count==0);
 bad=v0;bad[13]=0;
 assert(text_fields::decode(bad.data(),bad.size(),"vario451").count==0);
 bad=v0;bad[0]=0;
 assert(text_fields::decode(bad.data(),bad.size(),"vario451").count==0);
 assert(text_fields::decode(v0.data(),v0.size(),"compact5").count==0);
}
