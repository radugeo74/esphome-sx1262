// Original upstream QHeat V2 Mode0 and Zenner C5ISF error-state Mode0.
// Source fixture IDs are public; no meter private IDs/keys are used.
#include "components/wmbus_native_sx126x/xmq_batch2_plain_dispatch.h"
#include "components/wmbus_native_sx126x/xmq_auto_registry.h"
#include "components/wmbus_native_sx126x/xmq_plain_text_fields.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <vector>
static std::vector<uint8_t> hex(const char *s) {
 std::vector<uint8_t> out;int hi=-1;
 for(;*s;++s){if(*s=='_'||*s==' ')continue;
 int n=*s>='0'&&*s<='9'?*s-'0':*s>='a'&&*s<='f'?*s-'a'+10:
       *s>='A'&&*s<='F'?*s-'A'+10:-1;
 assert(n>=0);if(hi<0)hi=n;
 else {out.push_back(static_cast<uint8_t>((hi<<4)|n));hi=-1;}}
 assert(hi<0);return out;
}
static bool has(const xmq_portable::DecodedPayload&out,
                const char*name,double x) {
 for(size_t i=0;i<out.value_count;++i)
 if(out.values[i].name && std::strcmp(out.values[i].name,name)==0 &&
    std::fabs(out.values[i].value-x)<1e-5)return true;
 return false;
}
int main() {
 using namespace xmq_portable;
 const auto q0=hex("3C449344957002372337725880226793442304DC0000200C05043900004C0500000000426C9F2CCC080551070000C2086CBE29326CFFFF046D280DB62A");
 const auto q1=hex("58449344414620684737780779414620689344470c0dff5f3500825a00000e0007c00dffff310803001f3c036800003e34310803000080008000800080008000800000931a92293128190c00002f02fd170000046d000f3235");
 const auto q2=hex("414493444374266848377243742668934448042A000020_0C06140000004C0600000000426CFFFFCC080600000000C2086C3C3202FD170000326CFFFF046D190F3533");
 const auto q3=hex("5844934443742668483778_077943742668934448040DFF5F3500822A0000610107C006FFFF14000000FFFF000000003C32000000000080008000800080008000800080008000800080008000002F02FD170000046D1B0F3533");
 const auto q4=hex("58449344798995673E377807797989956793443E040DFF5F3500827D0000810007C006FFFF531100003F3C490900005F318010000083003C0022000B0000000000000000000A002B00510083002F02FD170000046D16085832");
 const auto q5=hex("41449344905898673e37729058986793443e04120000200c06809300004c0678820000426c3f3ccc080636900000c2086c5f3102fd170000326cffff046d1d095832");
 const auto q6=hex("58449344478491673C377807794784916793443C040DFF5F350082B40035F4E5CF5874D4A45106F8435DDA926DE66FE62E8ECCB25B199CCAE33C6B679AD69AC8629398ECDCB1DDB263EDA12137EA02FD170000046D080F4333");
 const auto q7=hex("58449344530175683F377807795301756893443F040DFF5F3500822C003555280D40FBA8998C9FE06BD8CD6796ED62F9C8B2CAE1A80D6166007C446865E9502913B1DC1EE820986D102033CE54B902FD170000046D22105639");
 const auto q8=hex("58449344409436683F377807794094366893443F040DFF5F350082110035D3B5EA13344D42345FD6C9B468DC8A7FC0A56DCECD9861481F7C87A367564D55E1576B5A9B00E00CF10466C038D6393902FD170000046D00135832");
 const auto q9=hex("2e4465B24509256723047aa20000000c05035612004c0520131100426c3f3c326cffff046d12014a330c7834980437");
 const auto c0=hex("E544496A55554455880D7A320200002F2F_04060000000004130000000002FD17240084800106000000008280016C2124C480010600000080C280016CFFFF84810106000000808281016CFFFFC481010600000080C281016CFFFF84820106000000808282016CFFFFC482010600000080C282016CFFFF84830106000000808283016CFFFFC483010600000080C283016CFFFF84840106000000808284016CFFFFC484010600000080C284016CFFFF84850106000000808285016CFFFFC485010600000080C285016CFFFF84860106000000808286016CFFFFC486010600000080C286016CFFFF");
 const auto c1=hex("5E44496A4420003288047AFC0050052F2F_0406D00E00000413B28A05008404060000000082046CC121043B00000000042D000000000259E719025D051402FD17000084800106C00C00008280016CC125948001AE25090000002F2F2F2F2F2F");
 const auto c2=hex("8E44496A03270232880D7AED0080052F2F0406050200008410069A1700000413BC6F2E00840406E701000082046C2131841406C20B000082146C2131043B00000000042D0000000002595D09025D240802FD17000084800106050200008280016C213C948001AE2501000000849001069A1700008290016C213C849001AE25000000002F2F2F2F2F2F2F2F2F2F2F2F");
 CalcValue vals[16]{};CalendarDate dates[2]{};Capture caps[12]{};
 DecodedPayload out{vals,16,0,dates,2,0};
 const auto decode=[&](const std::vector<uint8_t>&f,const char*id,const char*name){
 return batch2_plain::decode_configured(f.data(),f.size(),id,id,name,caps,12,out);
 };
 const auto automatic=[&](const std::vector<uint8_t>&f,const char*id){
 return batch2_plain::select_configured_driver(f.data(),f.size(),id,id,"auto",
 wmbus_pure::generated::signatures,wmbus_pure::generated::signature_count);
 };
 assert(automatic(q9,"67250945") &&
        std::strcmp(automatic(q9,"67250945"),"qheatv2")==0);
 assert(automatic(c0,"55445555") &&
        std::strcmp(automatic(c0,"55445555"),"c5isf")==0);
 assert(decode(q9,"67250945","qheatv2")==PureDecodeStatus::Ok);
 assert(out.value_count==2);
 assert(has(out,"total_kwh",12560.3));
 assert(has(out,"target_year_kwh",11132));
 assert(decode(c0,"55445555","c5isf")==PureDecodeStatus::Ok);
 assert(out.value_count==3);
 assert(has(out,"total_energy_consumption_kwh",0));
 assert(has(out,"total_volume_m3",0));
 assert(has(out,"total_energy_consumption_last_month_kwh",0));
 // Source C5ISF error status must not be misreported as OK.
 const auto cs=text_fields::decode(c0.data(),c0.size(),"c5isf");
 assert(cs.count==1);
 assert(std::strcmp(cs.fields[0].text,
        "DRY ERROR RETURN_SENSOR_INTERRUPTED")==0);
 const auto qs=text_fields::decode(q9.data(),q9.size(),"qheatv2");
 assert(qs.count==1 && std::strcmp(qs.fields[0].text,"OK")==0);
 // Previous frame kinds have no RF plaintext permission from NOKEY.
 for(const auto&v:{q0,q1,q2,q3,q4,q5,q6,q7,q8}){
  assert(decode(v,"67250945","qheatv2")!=PureDecodeStatus::Ok);
  assert(out.value_count==0);
 }
 for(const auto&v:{c1,c2}){
  assert(decode(v,"55445555","c5isf")!=PureDecodeStatus::Ok);
  assert(out.value_count==0);
 }
 auto bad=q9;bad[14]=0x25;
 assert(decode(bad,"67250945","qheatv2")!=PureDecodeStatus::Ok);
 bad=q9;bad[16]=0x06;
 assert(decode(bad,"67250945","qheatv2")!=PureDecodeStatus::Ok);
 bad=q9;bad[8]=0x24;
 assert(decode(bad,"67250945","qheatv2")!=PureDecodeStatus::Ok);
 bad=q9;bad[3]=0x32; // do not silently broaden M-field alias
 assert(decode(bad,"67250945","qheatv2")!=PureDecodeStatus::Ok);
 bad=q9;bad[0]++;
 assert(decode(bad,"67250945","qheatv2")!=PureDecodeStatus::Ok);
 bad=c0;bad[14]=0x25;
 assert(decode(bad,"55445555","c5isf")!=PureDecodeStatus::Ok);
 bad=c0;bad[9]=0x04;
 assert(decode(bad,"55445555","c5isf")!=PureDecodeStatus::Ok);
 bad=c0;bad[32]=0x25; // unknown error flag pattern: never claim OK
 assert(text_fields::decode(bad.data(),bad.size(),"c5isf").count==0);
 bad=c0;bad[17]=0x0C;
 assert(decode(bad,"55445555","c5isf")!=PureDecodeStatus::Ok);
 bad=c0;bad[0]++;
 assert(decode(bad,"55445555","c5isf")!=PureDecodeStatus::Ok);
 assert(decode(q9,"67250944","qheatv2")!=PureDecodeStatus::Ok);
 assert(decode(c0,"55445556","c5isf")!=PureDecodeStatus::Ok);
 assert(out.value_count==0);
}
