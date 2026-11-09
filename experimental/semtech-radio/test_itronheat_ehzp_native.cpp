// Public original Itron Heat and EMH eHZP short TPL Mode-0 fixtures.
#include "components/wmbus_native_sx126x/xmq_batch2_plain_dispatch.h"
#include "components/wmbus_native_sx126x/xmq_auto_registry.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <vector>
static std::vector<uint8_t> hex(const char*s) {
 std::vector<uint8_t> v;int a=-1;
 for(;*s;++s){if(*s=='_'||*s==' ')continue;
 int n=(*s>='0'&&*s<='9')?*s-'0':*s>='a'&&*s<='f'?*s-'a'+10:
       *s>='A'&&*s<='F'?*s-'A'+10:-1;
 assert(n>=0);if(a<0)a=n;else{
  v.push_back(static_cast<uint8_t>((a<<4)|n));a=-1;}}
 assert(a<0);return v;
}
static bool has(const xmq_portable::DecodedPayload&out,
                const char*name,double x) {
 for(size_t i=0;i<out.value_count;++i)
 if(out.values[i].name && std::strcmp(out.values[i].name,name)==0 &&
    std::fabs(out.values[i].value-x)<0.00001)return true;
 return false;
}
int main() {
 using namespace xmq_portable;
 const auto i0=hex("444497268504342300047AD00030A52F2F_04062C0100000C1429270000066D2D130AE12B007406FEFEFEFE426C1F010D7FEB0E00000006040C995500372F2F0C7951622223");
 const auto i1=hex("404497A65757751100047A040000A00406494C0000041348318000066D2B0ED7EE2A0044069A360000426CDF2C0D7FEB0700000006040C5C7E034E0C7942502321");
 const auto e0=hex("5344A8159955995502028C201D900F002C250C390000ED176BBBB1591ADB7A1D003007102F2F_0700583B74020000000007803CBCD70200000000000728B070200000000000042092A406002F2F2F2F2F2F2F2F2F");
 const auto e1=hex("AE44a8153412711300027a010000002F2F0700285EB5050000000007803C34080000000000000728905F01000000000004208F84160101FD971D00C1017402C401208D841601C701031C76010000000000CD01831324410800010000000001000000000100000001000000000100000000010000000001000000C701833C0200000000000000CD0183933C244108000000000000000000000000000000000000000000000000000000000000000000");
 CalcValue values[16]{};CalendarDate dates[2]{};Capture caps[12]{};
 DecodedPayload out{values,16,0,dates,2,0};
 const auto decode=[&](const std::vector<uint8_t>&f,const char*id,const char*driver){
   return batch2_plain::decode_configured(f.data(),f.size(),id,id,driver,caps,12,out);
 };
 const auto automatic=[&](const std::vector<uint8_t>&f,const char*id){
  return batch2_plain::select_configured_driver(f.data(),f.size(),id,id,"auto",
         wmbus_pure::generated::signatures,wmbus_pure::generated::signature_count);
 };
 assert(automatic(i1,"11755757") &&
        std::strcmp(automatic(i1,"11755757"),"itronheat")==0);
 assert(automatic(e1,"13711234") &&
        std::strcmp(automatic(e1,"13711234"),"ehzp")==0);
 assert(decode(i1,"11755757","itronheat")==PureDecodeStatus::Ok);
 assert(out.value_count==3);
 assert(has(out,"total_kwh",19529));
 assert(has(out,"total_m3",8401.224));
 assert(has(out,"last_year_kwh",13978));
 assert(decode(e1,"13711234","ehzp")==PureDecodeStatus::Ok);
 assert(out.value_count==3);
 assert(has(out,"total_energy_consumption_kwh",95.7722));
 assert(has(out,"total_energy_production_kwh",0.0021));
 assert(has(out,"current_power_consumption_kw",0.09));
 // NOKEY does not make a Mode5 on-air frame or ELL CI8C clear RF.
 assert(decode(i0,"23340485","itronheat")!=PureDecodeStatus::Ok);
 assert(out.value_count==0);
 assert(decode(e0,"55995599","ehzp")!=PureDecodeStatus::Ok);
 assert(out.value_count==0);
 auto bad=i1;bad[14]=0xA5;bad[13]=0x30;
 assert(decode(bad,"11755757","itronheat")!=PureDecodeStatus::Ok);
 bad=i1;bad[16]=0x04;
 assert(decode(bad,"11755757","itronheat")!=PureDecodeStatus::Ok);
 bad=i1;bad[0]++;
 assert(decode(bad,"11755757","itronheat")!=PureDecodeStatus::Ok);
 bad=i1;bad[9]=0x0D;
 assert(decode(bad,"11755757","itronheat")!=PureDecodeStatus::Ok);
 bad=e1;bad[14]=0x25;
 assert(decode(bad,"13711234","ehzp")!=PureDecodeStatus::Ok);
 bad=e1;bad[28]=0x00;
 assert(decode(bad,"13711234","ehzp")!=PureDecodeStatus::Ok);
 bad=e1;bad[39]=0x29;
 assert(decode(bad,"13711234","ehzp")!=PureDecodeStatus::Ok);
 bad=e1;bad[58]=0x01;
 assert(decode(bad,"13711234","ehzp")!=PureDecodeStatus::Ok);
 bad=e1;bad[9]=0x03;
 assert(decode(bad,"13711234","ehzp")!=PureDecodeStatus::Ok);
 bad=e1;bad[0]++;
 assert(decode(bad,"13711234","ehzp")!=PureDecodeStatus::Ok);
 assert(decode(i1,"11755758","itronheat")!=PureDecodeStatus::Ok);
 assert(decode(e1,"13711233","ehzp")!=PureDecodeStatus::Ok);
 assert(out.value_count==0);
}
