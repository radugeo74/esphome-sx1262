// Strict original long-TPL CI72 source samples with distinct DLL/TPL IDs.
// Never relax the normal single-address publication gate.
#include "components/wmbus_native_sx126x/xmq_batch2_plain_dispatch.h"
#include "components/wmbus_native_sx126x/xmq_auto_registry.h"
#include "components/wmbus_native_sx126x/native_publication_gate.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <vector>
static std::vector<uint8_t> hex(const char*s){
 std::vector<uint8_t> v;int high=-1;
 for(;*s;++s){if(*s=='_'||*s==' ')continue;
 int n=*s>='0'&&*s<='9'?*s-'0':*s>='a'&&*s<='f'?*s-'a'+10:
       *s>='A'&&*s<='F'?*s-'A'+10:-1;
 assert(n>=0);if(high<0)high=n;else{
   v.push_back(static_cast<uint8_t>((high<<4)|n));high=-1;}}
 assert(high<0);return v;
}
static bool has(const xmq_portable::DecodedPayload&o,const char*name,double expected){
 for(size_t i=0;i<o.value_count;++i)if(o.values[i].name &&
   std::strcmp(o.values[i].name,name)==0 &&
   std::fabs(o.values[i].value-expected)<1e-5)return true;
 return false;
}
int main(){
 using namespace xmq_portable;
 const auto e0=hex("51440186010905001837721956880101064004DA000020026CA9220E017799241103000C13641320000A2D00000A5A90060A5E800544050E77000001FD0C010A6564370AFD4731030A274907047F00000002");
 const auto w0=hex("8f44c5145502004301077260402520c51400076b0000002f2f426cbf2c441322e9000001fd17008401133c340100c40113ae2d010084021303290100c402137e21010084031313180100c403138a0e010084041337060100c40413b2fc00008405139af30000c4051322e90000840613c1df0000c40613cdd5000084071365ce0000c407136dc500008408138dbf0000");
 const auto w1=hex("7644c5145528502202077320248622c5140007700060255f2aa2967eb77d0bf310341e3636d9acc17cc5d69ce5632d7b494cc0718e2dbb916bbec0c3ff2b0d6087fd0abb3ba0779564cc089e4527987ef2221d26d7ea448ca59759acfbea4e5b0026ef463592b6668fbf680896e78b461dbb77c32903e0");
 CalcValue vals[24]{};CalendarDate dates[2]{};Capture caps[12]{};
 DecodedPayload out{vals,24,0,dates,2,0};
 const auto decode=[&](const std::vector<uint8_t>&f,
    const char*target,const char*dll,const char*driver){
  return batch2_plain::decode_configured(
         f.data(),f.size(),target,dll,driver,caps,12,out);
 };
 const auto automatic=[&](const std::vector<uint8_t>&f,
    const char*target,const char*dll){
  return batch2_plain::select_configured_driver(
         f.data(),f.size(),target,dll,"auto",
         wmbus_pure::generated::signatures,
         wmbus_pure::generated::signature_count);
 };
 assert(automatic(e0,"01885619","00050901") &&
        std::strcmp(automatic(e0,"01885619","00050901"),"elf")==0);
 assert(automatic(w0,"20254060","43000255") &&
        std::strcmp(automatic(w0,"20254060","43000255"),"engelmann_faw")==0);
 assert(decode(e0,"01885619","00050901","elf")==PureDecodeStatus::Ok);
 assert(out.value_count==5);
 assert(has(out,"total_energy_consumption_kwh",3112.49977));
 assert(has(out,"total_volume_m3",201.364));
 assert(has(out,"total_energy_consumption_at_date_kwh",3047.8));
 assert(has(out,"flow_temperature_c",69));
 assert(has(out,"return_temperature_c",58));
 assert(decode(w0,"20254060","43000255","engelmann_faw")==PureDecodeStatus::Ok);
 assert(out.value_count==3);
 assert(has(out,"consumption_at_reporting_date_m3",59.682));
 assert(has(out,"consumption_1_months_ago_m3",78.908));
 assert(has(out,"consumption_2_months_ago_m3",77.23));
 // The strict legacy publication::inspect() must still reject differing
 // long-TPL identity. Only the new exact TWO-ID route accepts it.
 assert(wmbus_pure::publication::inspect(e0.data(),e0.size(),50901)!=
        wmbus_pure::publication::Decision::Allowed);
 assert(wmbus_pure::publication::inspect(w0.data(),w0.size(),43000255)!=
        wmbus_pure::publication::Decision::Allowed);
 assert(decode(w1,"22862420","43000255","engelmann_faw")!=PureDecodeStatus::Ok);
 assert(out.value_count==0);
 assert(!automatic(e0,"01885619","01885619"));
 assert(!automatic(w0,"20254060","20254060"));
 assert(decode(e0,"01885619","00050902","elf")!=PureDecodeStatus::Ok);
 assert(decode(e0,"01885620","00050901","elf")!=PureDecodeStatus::Ok);
 assert(decode(w0,"20254060","43000256","engelmann_faw")!=PureDecodeStatus::Ok);
 assert(decode(w0,"20254061","43000255","engelmann_faw")!=PureDecodeStatus::Ok);
 auto bad=e0;bad[22]=0x25; // Mode-5 AES, not plaintext
 assert(decode(bad,"01885619","00050901","elf")!=PureDecodeStatus::Ok);
 bad=e0;bad[15]=0x02; // fake TPL manufacturer
 assert(decode(bad,"01885619","00050901","elf")!=PureDecodeStatus::Ok);
 bad=e0;bad[27]=0x04; // DIF profile mismatch
 assert(decode(bad,"01885619","00050901","elf")!=PureDecodeStatus::Ok);
 bad=e0;bad[29]=0xFA; // invalid BCD
 assert(decode(bad,"01885619","00050901","elf")!=PureDecodeStatus::Ok);
 bad=e0;bad[0]++;
 assert(decode(bad,"01885619","00050901","elf")!=PureDecodeStatus::Ok);
 bad=w0;bad[22]=0x25;
 assert(decode(bad,"20254060","43000255","engelmann_faw")!=PureDecodeStatus::Ok);
 bad=w0;bad[17]=0x01; // different TPL version
 assert(decode(bad,"20254060","43000255","engelmann_faw")!=PureDecodeStatus::Ok);
 bad=w0;bad[39]=0x04;
 assert(decode(bad,"20254060","43000255","engelmann_faw")!=PureDecodeStatus::Ok);
 bad=w0;bad[38]=0x01; // unknown application error flag
 assert(decode(bad,"20254060","43000255","engelmann_faw")!=PureDecodeStatus::Ok);
 bad=w0;bad[0]++;
 assert(decode(bad,"20254060","43000255","engelmann_faw")!=PureDecodeStatus::Ok);
 assert(out.value_count==0);
}
