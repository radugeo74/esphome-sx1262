// Public upstream itron.xmq and qualcosonic.xmq Mode-0 examples.
#include "components/wmbus_native_sx126x/xmq_batch2_plain_dispatch.h"
#include "components/wmbus_native_sx126x/xmq_auto_registry.h"
#include "components/wmbus_native_sx126x/xmq_plain_text_fields.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <vector>
static std::vector<uint8_t> hex(const char*s) {
 std::vector<uint8_t> v;int hi=-1;
 for(;*s;++s){if(*s=='_'||*s==' ')continue;
 const int n=(*s>='0'&&*s<='9')?*s-'0':
             (*s>='a'&&*s<='f')?*s-'a'+10:
             (*s>='A'&&*s<='F')?*s-'A'+10:-1;
 assert(n>=0);if(hi<0)hi=n;else{
  v.push_back(static_cast<uint8_t>((hi<<4)|n));hi=-1;}}
 assert(hi<0);return v;
}
static bool has(const xmq_portable::DecodedPayload &out,
                const char *field,double expected) {
 for(size_t i=0;i<out.value_count;++i)
  if(out.values[i].name && std::strcmp(out.values[i].name,field)==0 &&
     std::fabs(out.values[i].value-expected)<0.00001)return true;
 return false;
}
int main(){
 using namespace xmq_portable;
 const auto i0=hex("384497269856341203077AD90000A0_0413FD110000066D2C1AA1D521004413300F0000426CBF2C047F0000060C027F862A0E79678372082100");
 const auto i1=hex("46449726560000183307725600001897263307AF0030052F2F_066D0E1015C82A000C13771252000C933C000000000B3B0400004C1361045200426CC12A03FD971C0000002F2F2F");
 const auto i2=hex("384497265909312000077a930000a0041360B50100066d101295f427004413ac570100426cdf2c047f0000060c027f6c2a0e79000000000000");
 const auto i3=hex("3A4497269820362300167AF60020A52F2F_04132E100000066D03260DE12B007413FEFEFEFE426C1F01047F1600060C027F9A2A0E79187103002300");
 const auto q0=hex("76440907086401030B0d7a78000000046d030fB726346d0000010134fd170000000004200f13cf0104240f13cf0104863B0000000004863cdc0000000413B5150600042B86f1ffff043B030200000259c002025d2c05026194fd0c780864010384086d3B17Bf258408863B000000008408863c0B000000");
 const auto q1=hex("76440907021048290C0D7A89000000_046D2301E122346D0000010134FD17000000000420097468020424DE736802048E3BF1190000048E3C00000000041323860100042B6E050000043B2500000002598016025D000A0261800C0C780210482984086D3B17FF2184088E3BED19000084088E3C00000000");
 const auto q2=hex("9A440907859449980C0D7A8C000000_046D3201E122346D0000010134FD17000000000420BB776802042490776802048E3BF2190000048E3C00000000041329860100042B6A010000043B0B0000000259F615025D1E0B0261D80A0C788594499884086D3B17FF21820859BC1382085D720B840824915D680284088E3BED19000084088E3C00000000840813028601008408BE5800000000027F00AD");
 CalcValue values[16]{};CalendarDate dates[2]{};Capture caps[12]{};
 DecodedPayload out{values,16,0,dates,2,0};
 const auto decode=[&](const std::vector<uint8_t>&frame,
                       const char *id,const char *driver){
  return batch2_plain::decode_configured(frame.data(),frame.size(),
                                         id,id,driver,caps,12,out);
 };
 const auto automatic=[&](const std::vector<uint8_t>&frame,const char *id){
  return batch2_plain::select_configured_driver(
   frame.data(),frame.size(),id,id,"auto",
   wmbus_pure::generated::signatures,wmbus_pure::generated::signature_count);
 };
 assert(automatic(i0,"12345698") &&
        std::strcmp(automatic(i0,"12345698"),"itron")==0);
 assert(automatic(q0,"03016408") &&
        std::strcmp(automatic(q0,"03016408"),"qualcosonic")==0);
 assert(decode(i0,"12345698","itron")==PureDecodeStatus::Ok);
 assert(out.value_count==2);
 assert(has(out,"total_m3",4.605));
 assert(has(out,"target_m3",3.888));
 const auto itron_status=text_fields::decode(i0.data(),i0.size(),"itron");
 assert(itron_status.count==1 && std::strcmp(itron_status.fields[0].text,"OK")==0);
 assert(decode(i2,"20310959","itron")==PureDecodeStatus::Ok);
 assert(out.value_count==2);
 assert(has(out,"total_m3",111.968));
 assert(has(out,"target_m3",87.98));
 assert(decode(q0,"03016408","qualcosonic")==PureDecodeStatus::Ok);
 assert(out.value_count==4);
 assert(has(out,"total_m3",398.773));
 assert(has(out,"flow_temperature_c",7.04));
 assert(has(out,"return_temperature_c",13.24));
 assert(has(out,"power_kw",-3.706));
 const auto qual_status=text_fields::decode(q0.data(),q0.size(),"qualcosonic");
 assert(qual_status.count==1 && std::strcmp(qual_status.fields[0].text,"OK")==0);
 assert(decode(q1,"29481002","qualcosonic")==PureDecodeStatus::Ok);
 assert(out.value_count==4);
 assert(has(out,"total_m3",99.875));
 assert(has(out,"flow_temperature_c",57.6));
 assert(has(out,"return_temperature_c",25.6));
 assert(has(out,"power_kw",1.39));
 assert(decode(q2,"98499485","qualcosonic")==PureDecodeStatus::Ok);
 assert(out.value_count==4);
 assert(has(out,"total_m3",99.881));
 assert(has(out,"flow_temperature_c",56.22));
 assert(has(out,"return_temperature_c",28.46));
 assert(has(out,"power_kw",0.362));
 // Never accept nested/gateway frame, or supplied-clear AES example.
 assert(decode(i1,"18000056","itron")!=PureDecodeStatus::Ok);
 assert(out.value_count==0);
 assert(decode(i3,"23362098","itron")!=PureDecodeStatus::Ok);
 assert(out.value_count==0);
 assert(text_fields::decode(i3.data(),i3.size(),"itron").count==0);
 auto bad=i0;bad[14]=0xA5;
 assert(decode(bad,"12345698","itron")!=PureDecodeStatus::Ok);
 bad=i0;bad[16]=0x6E;
 assert(decode(bad,"12345698","itron")!=PureDecodeStatus::Ok);
 bad=i0;bad[35]=0x4C;
 assert(decode(bad,"12345698","itron")!=PureDecodeStatus::Ok);
 bad=i0;bad[0]++;
 assert(decode(bad,"12345698","itron")!=PureDecodeStatus::Ok);
 bad=q0;bad[13]=0x50;bad[14]=0x05;
 assert(decode(bad,"03016408","qualcosonic")!=PureDecodeStatus::Ok);
 bad=q0;bad[9]=0x07;
 assert(decode(bad,"03016408","qualcosonic")!=PureDecodeStatus::Ok);
 bad=q0;bad[61]=0x14; // corrupt volume VIF
 assert(decode(bad,"03016408","qualcosonic")!=PureDecodeStatus::Ok);
 assert(decode(i0,"12345697","itron")!=PureDecodeStatus::Ok);
 assert(decode(q0,"03016409","qualcosonic")!=PureDecodeStatus::Ok);
 assert(out.value_count==0);
}
