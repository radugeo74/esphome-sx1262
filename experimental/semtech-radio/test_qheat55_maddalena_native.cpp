// Original public XMQ QHeat 5.5 US and Maddalena CI7A Mode-0 regression.
#include "components/wmbus_native_sx126x/xmq_batch2_plain_dispatch.h"
#include "components/wmbus_native_sx126x/xmq_auto_registry.h"
#include "components/wmbus_native_sx126x/xmq_plain_text_fields.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <vector>
static std::vector<uint8_t> from_hex(const char*s) {
 std::vector<uint8_t> v;int hi=-1;
 for(;*s;++s){if(*s=='_'||*s==' ')continue;
 int x=*s>='0'&&*s<='9'?*s-'0':*s>='a'&&*s<='f'?*s-'a'+10:
       *s>='A'&&*s<='F'?*s-'A'+10:-1;
 assert(x>=0);if(hi<0)hi=x;else{
   v.push_back(static_cast<uint8_t>((hi<<4)|x));hi=-1;}}
 assert(hi<0);return v;
}
static bool has(const xmq_portable::DecodedPayload&out,
                const char*name,double x){
 for(size_t i=0;i<out.value_count;++i)
 if(out.values[i].name && std::strcmp(out.values[i].name,name)==0 &&
   std::fabs(out.values[i].value-x)<0.00001)return true;
 return false;
}
int main(){
 using namespace xmq_portable;
 const auto q0=from_hex("9644a732845483700a047ae70000200274fc00046d230Bd3250c0605920000446d3B17Bf2c4c068251000084016d3B17de248c010605900000cc0106988500008c020629770000cc0206226600008c030682510000cc0306933600008c040602260000cc0406691800008c050633140000cc0506900900008c060618020000cc0606000000008c0706000000003c22000000000f001000");
 const auto q1=from_hex("9644a7323896637007047a5B00002002740400046d2a0fe2210c0600080600446d3B17df2c4c069106060084016d3B17df2c8c010691060600cc0106227005008c020667430500cc0206122505008c030612180500cc0306821305008c040636090500cc0406080505008c050660960400cc0506597304008c060600410400cc0606790504008c0706936303003c22000000000f001000");
 const auto q2=from_hex("4544A732801907290A047AFB1000200C06063701000C14229705008C0106843601008C01142882050084016D3B179F284C0668020100446D3B179F2C046D2708B7290F000001");
 const auto q3=from_hex("3844a732785634120a047a400000200c06000000008c01060000000084016d0000413a4c0600000000446d00004131046d350c413a0f000000");
 const auto m0=from_hex("4E4424349986012401077AF2000020_2F2F0413A7000000046D0E0C163B04FD17000000000E789986012401FF441300000000426C01018401134A00000082016C1F3AD3013B470500C4016D1B14153B");
 CalcValue vals[16]{};CalendarDate dates[2]{};Capture caps[12]{};
 DecodedPayload out{vals,16,0,dates,2,0};
 const auto decode=[&](const std::vector<uint8_t>&f,const char*id,const char*name){
  return batch2_plain::decode_configured(f.data(),f.size(),id,id,name,caps,12,out);
 };
 const auto automatic=[&](const std::vector<uint8_t>&f,const char*id){
  return batch2_plain::select_configured_driver(f.data(),f.size(),id,id,"auto",
     wmbus_pure::generated::signatures,wmbus_pure::generated::signature_count);
 };
 assert(automatic(q0,"70835484") && std::strcmp(automatic(q0,"70835484"),"qheat_55_us")==0);
 assert(automatic(m0,"24018699") && std::strcmp(automatic(m0,"24018699"),"maddalena")==0);
 assert(decode(q0,"70835484","qheat_55_us")==PureDecodeStatus::Ok);
 assert(has(out,"total_energy_consumption_kwh",9205));
 assert(has(out,"key_datetime_kwh",5182));
 
 assert(decode(q1,"70639638","qheat_55_us")==PureDecodeStatus::Ok);
 assert(has(out,"total_energy_consumption_kwh",60800));
 assert(has(out,"key_datetime_kwh",60691));
 
 assert(decode(q2,"29071980","qheat_55_us")==PureDecodeStatus::Ok);
 assert(has(out,"total_energy_consumption_kwh",13706));
 assert(has(out,"key_datetime_kwh",10268));
 assert(has(out,"total_m3",597.22));
 assert(decode(q3,"12345678","qheat_55_us")==PureDecodeStatus::Ok);
 assert(has(out,"total_energy_consumption_kwh",0));
 assert(has(out,"key_datetime_kwh",0));
 
 assert(decode(m0,"24018699","maddalena")==PureDecodeStatus::Ok);
 assert(out.value_count==2);
 assert(has(out,"total_m3",0.167));
 assert(has(out,"target_m3",0));
 // BCD corruption, wrong manufacturer/MVT, encrypted header, frame truncation
 // and mismatched configured DLL IDs all fail before publication.
 const auto qstatus=text_fields::decode(q2.data(),q2.size(),"qheat_55_us");
 assert(qstatus.count==0); // F0 error fixture must NEVER be misreported OK
 const auto mstatus=text_fields::decode(m0.data(),m0.size(),"maddalena");
 assert(mstatus.count==1 && std::strcmp(mstatus.fields[0].text,"OK")==0);
 assert(decode(q0,"70835485","qheat_55_us")!=PureDecodeStatus::Ok);
 assert(decode(m0,"24018698","maddalena")!=PureDecodeStatus::Ok);
 auto bad=q0;bad[13]=0;bad[14]=0x25;
 assert(decode(bad,"70835484","qheat_55_us")!=PureDecodeStatus::Ok);
 bad=q0;bad[9]=0x0D;
 assert(decode(bad,"70835484","qheat_55_us")!=PureDecodeStatus::Ok);
 bad=q0;bad[0]++;
 assert(decode(bad,"70835484","qheat_55_us")!=PureDecodeStatus::Ok);
 bad=q0;bad[27]=0xFA;
 assert(decode(bad,"70835484","qheat_55_us")!=PureDecodeStatus::Ok);
 bad=m0;bad[14]=0x25;
 assert(decode(bad,"24018699","maddalena")!=PureDecodeStatus::Ok);
 bad=m0;bad[9]=0x06; // detect.mvt sees warm water, but no verified layout
 assert(decode(bad,"24018699","maddalena")!=PureDecodeStatus::Ok);
 bad=m0;bad[18]=0x14;
 assert(decode(bad,"24018699","maddalena")!=PureDecodeStatus::Ok);
 bad=m0;bad[45]=0x14;
 assert(decode(bad,"24018699","maddalena")!=PureDecodeStatus::Ok);
 bad=m0;bad[0]++;
 assert(decode(bad,"24018699","maddalena")!=PureDecodeStatus::Ok);
 assert(out.value_count==0);
}
