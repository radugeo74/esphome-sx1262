// Genuine upstream Apator E-ITN and Gran Systems T1/C1 plaintext
// original frames. Reject unknown CI78 models and unverified B6 layouts.
#include "components/wmbus_native_sx126x/xmq_batch2_plain_dispatch.h"
#include "components/wmbus_native_sx126x/xmq_auto_registry.h"
#include "components/wmbus_native_sx126x/xmq_plain_text_fields.h"
#include "components/wmbus_native_sx126x/native_publication_gate.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <vector>
static std::vector<uint8_t> hex(const char*s) {
 std::vector<uint8_t> out;int hi=-1;
 for(;*s;++s){if(*s=='_'||*s==' ')continue;
 int x=*s>='0'&&*s<='9'?*s-'0':*s>='a'&&*s<='f'?*s-'a'+10:
     *s>='A'&&*s<='F'?*s-'A'+10:-1;
 assert(x>=0);if(hi<0)hi=x;else{
 out.push_back(static_cast<uint8_t>((hi<<4)|x));hi=-1;}}
 assert(hi<0);return out;
}
static bool has(const xmq_portable::DecodedPayload&o,const char*n,double v) {
 for(size_t i=0;i<o.value_count;++i)
 if(o.values[i].name && std::strcmp(o.values[i].name,n)==0 &&
    std::fabs(o.values[i].value-v)<0.000001)return true;
 return false;
}
int main() {
 using namespace xmq_portable;
 const auto a0=hex("19440186313737370408A0A1000059001C270100322DE413B415");
 const auto a1=hex("25441486323737370408B60AFFFFF5450186F41B9D58A0A100007809000000001F2D6416C819");
 const auto a2=hex("29441486333737370408B60EFFFFF1460186EC1B934EE91BA57BA0A1000059009C250100322DE413B415");
 const auto g0=hex("7844731e78610418010278046d0f13bc21040394030000841003690300008420032b00000084300300000000848010030000000084016d0000bc2184010394030000841103690300008421032b00000084310300000000848110030000000004fd482e09000004fd5b0000000002fb2d861304fd1700000201");
 const auto g1=hex("9e44731e17011020010278046d0813bc21040300000000841003000000008420030000000084300300000000848010030000000084016d0000bc218401030000000084110300000000842103000000008431030000000084811003000000008440fd4825090000848040fd480000000084c040fd48000000008440fd5b00000000848040fd5b0000000084c040fd5b0000000002fb2d881304fd1702000101");
 CalcValue values[16]{};CalendarDate dates[2]{};Capture caps[12]{};
 DecodedPayload out{values,16,0,dates,2,0};
 const auto decode=[&](const std::vector<uint8_t>&f,const char*id,
                       const char*driver) {
  return batch2_plain::decode_configured(f.data(),f.size(),id,id,driver,
                                         caps,12,out);
 };
 const auto automatic=[&](const std::vector<uint8_t>&f,const char*id) {
  return batch2_plain::select_configured_driver(f.data(),f.size(),id,id,"auto",
    wmbus_pure::generated::signatures,wmbus_pure::generated::signature_count);
 };
 for(const auto&pair:std::vector<std::pair<const std::vector<uint8_t>*,const char*>>{
   {&a0,"37373731"},{&a1,"37373732"},{&a2,"37373733"}}) {
  const auto selected=automatic(*pair.first,pair.second);
  assert(selected && std::strcmp(selected,"apatoreitn")==0);
 }
 for(const auto&pair:std::vector<std::pair<const std::vector<uint8_t>*,const char*>>{
   {&g0,"18046178"},{&g1,"20100117"}}) {
  const auto selected=automatic(*pair.first,pair.second);
  assert(selected && std::strcmp(selected,"gransystems")==0);
 }
 assert(decode(a0,"37373731","apatoreitn")==PureDecodeStatus::Ok);
 assert(out.value_count==4);
 assert(has(out,"current_hca",1));
 assert(has(out,"previous_hca",89));
 assert(has(out,"temp_room_avg_c",21.703125));
 assert(has(out,"temp_room_prev_avg_c",19.890625));
 const auto txt0=text_fields::decode(a0.data(),a0.size(),"apatoreitn");
 assert(txt0.count==1 && std::strcmp(txt0.fields[0].text,"SEAL_BROKEN")==0);
 assert(decode(a1,"37373732","apatoreitn")==PureDecodeStatus::Ok);
 assert(out.value_count==4);
 assert(has(out,"current_hca",0));
 assert(has(out,"previous_hca",2424));
 assert(has(out,"temp_room_avg_c",25.78125));
 assert(has(out,"temp_room_prev_avg_c",22.390625));
 const auto txt1=text_fields::decode(a1.data(),a1.size(),"apatoreitn");
 assert(txt1.count==1 && std::strcmp(txt1.fields[0].text,"OK")==0);
 assert(decode(a2,"37373733","apatoreitn")==PureDecodeStatus::Ok);
 assert(out.value_count==4);
 assert(has(out,"current_hca",1));
 assert(has(out,"previous_hca",89));
 assert(has(out,"temp_room_avg_c",21.703125));
 assert(has(out,"temp_room_prev_avg_c",19.890625));
 const auto txt2=text_fields::decode(a2.data(),a2.size(),"apatoreitn");
 assert(txt2.count==1 && std::strcmp(txt2.fields[0].text,"SEAL_BROKEN")==0);
 assert(decode(g0,"18046178","gransystems")==PureDecodeStatus::Ok);
 assert(out.value_count==3);
 assert(has(out,"total_energy_consumption_kwh",0.916));
 assert(has(out,"total_energy_consumption_tariff_1_kwh",0.873));
 assert(has(out,"total_energy_consumption_tariff_2_kwh",0.043));
 assert(text_fields::decode(g0.data(),g0.size(),"gransystems").count==0);
 assert(decode(g1,"20100117","gransystems")==PureDecodeStatus::Ok);
 assert(out.value_count==3);
 assert(has(out,"total_energy_consumption_kwh",0));
 assert(has(out,"total_energy_consumption_tariff_1_kwh",0));
 assert(has(out,"total_energy_consumption_tariff_2_kwh",0));
 assert(text_fields::decode(g1.data(),g1.size(),"gransystems").count==0);
 // 0x78 has no TPL security header. Existing generic admission must
 // remain rejecting it despite exact GSS decoder source fixtures.
 assert(wmbus_pure::publication::inspect(g0.data(),g0.size(),18046178)!=
        wmbus_pure::publication::Decision::Allowed);
 assert(wmbus_pure::publication::inspect(g1.data(),g1.size(),20100117)!=
        wmbus_pure::publication::Decision::Allowed);
 assert(decode(a0,"37373732","apatoreitn")!=PureDecodeStatus::Ok);
 assert(decode(g0,"18046179","gransystems")!=PureDecodeStatus::Ok);
 assert(out.value_count==0);
 auto bad=a0;bad[10]=0xA2;
 assert(decode(bad,"37373731","apatoreitn")!=PureDecodeStatus::Ok);
 bad=a0;bad[11]=0xA0;
 assert(decode(bad,"37373731","apatoreitn")!=PureDecodeStatus::Ok);
 bad=a0;bad[22]=0xFF;bad[23]=0xFF;
 assert(decode(bad,"37373731","apatoreitn")!=PureDecodeStatus::Ok);
 bad=a1;bad[11]=0x0B;
 assert(decode(bad,"37373732","apatoreitn")!=PureDecodeStatus::Ok);
 bad=a1;bad[22]=0x2F;
 assert(decode(bad,"37373732","apatoreitn")!=PureDecodeStatus::Ok);
 bad=a2;bad[10]=0x72;
 assert(decode(bad,"37373733","apatoreitn")!=PureDecodeStatus::Ok);
 bad=a2;bad[0]++;
 assert(decode(bad,"37373733","apatoreitn")!=PureDecodeStatus::Ok);
 bad=g0;bad[10]=0x7A;
 assert(decode(bad,"18046178","gransystems")!=PureDecodeStatus::Ok);
 bad=g0;bad[9]=0x07;
 assert(decode(bad,"18046178","gransystems")!=PureDecodeStatus::Ok);
 bad=g0;bad[18]=0x06;
 assert(decode(bad,"18046178","gransystems")!=PureDecodeStatus::Ok);
 bad=g0;bad[21]=0xFF;bad[22]=0xFF;
 assert(decode(bad,"18046178","gransystems")!=PureDecodeStatus::Ok);
 bad=g1;bad[153]=0x00;
 assert(decode(bad,"20100117","gransystems")!=PureDecodeStatus::Ok);
 bad=g1;bad[0]++;
 assert(decode(bad,"20100117","gransystems")!=PureDecodeStatus::Ok);
 assert(out.value_count==0);
}
