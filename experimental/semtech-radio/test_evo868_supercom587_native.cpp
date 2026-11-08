// Configured-only official upstream EVO868 + Supercom 587 clear TPL fixtures.
#include "components/wmbus_native_sx126x/xmq_batch2_plain_dispatch.h"
#include "components/wmbus_native_sx126x/xmq_auto_registry.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <vector>
static std::vector<uint8_t> from_hex(const char*s) {
 std::vector<uint8_t> v;int a=-1;
 for(;*s;++s){if(*s=='_'||*s==' ')continue;
 int n=*s>='0'&&*s<='9'?*s-'0':*s>='a'&&*s<='f'?*s-'a'+10:
       *s>='A'&&*s<='F'?*s-'A'+10:-1;
 assert(n>=0);if(a<0)a=n;
 else{v.push_back(static_cast<uint8_t>((a<<4)|n));a=-1;}}
 assert(a<0);return v;
}
static bool has(const xmq_portable::DecodedPayload&out,const char*name,double v){
 for(size_t i=0;i<out.value_count;++i)
 if(out.values[i].name && std::strcmp(out.values[i].name,name)==0 &&
    std::fabs(out.values[i].value-v)<0.00001)return true;
 return false;
}
int main(){
 using namespace xmq_portable;
 const auto evo1=from_hex("ae4424347677787950077ac10000202f2f041306070000046d1e31b12104fd17000000000e787880048120004413c9040000426c9f2c840113c904000082016c9f2cd3013b9a0200c4016d0534a7218104fd280182046c9f2c840413c9040000c404131b00000084051300000000c405130000000084061300000000c406130000000084071300000000c407130000000084081300000000c408130000000084091300000000c4091300000000ffff");
 const auto evo2=from_hex("AC4424349418032150167A39000020_2F2F041356850000046D0A340B3C04FD17000000000E786858000000004413D0620000426CFF2C8401137A83000082016C1E3BD3013BCF0200C4016D09210E338104FD280182046C1E3B8404137A830000C40413BE7E00008405131A7B0000C405136B7600008406132A700000C40613ED6A00008407130F690000C40713536800008408136E670000C40813EE65000084091330640000C40913D0620000");
 const auto superwarm=from_hex("A244EE4D785634123C067A8F000000_0C1348550000426CE1F14C130000000082046C21298C0413330000008D04931E3A3CFE3300000033000000330000003300000033000000330000003300000033000000330000003300000033000000330000004300000034180000046D0D0B5C2B03FD6C5E150082206C5C290BFD0F0200018C4079678885238310FD3100000082106C01018110FD610002FD66020002FD170000");
 const auto supercold=from_hex("A244EE4D111111113C077AAC000000_0C1389490000426CE1F14C130000000082046C21298C0413010000008D04931E3A3CFE0100000001000000010000000100000001000000010000000100000001000000010000000100000001000000010000001600000031130000046D0A0C5C2B03FD6C60150082206C5C290BFD0F0200018C4079629885238310FD3100000082106C01018110FD610002FD66020002FD170000");
 CalcValue values[16]{};CalendarDate dates[2]{};Capture caps[12]{};
 DecodedPayload out{values,16,0,dates,2,0};
 const auto decode=[&](const std::vector<uint8_t>&f,const char*id,const char*driver){
 return batch2_plain::decode_configured(f.data(),f.size(),id,id,driver,caps,12,out);
 };
 const auto auto_driver=[&](const std::vector<uint8_t>&f,const char*id){
 return batch2_plain::select_configured_driver(f.data(),f.size(),id,id,"auto",
   wmbus_pure::generated::signatures,wmbus_pure::generated::signature_count);
 };
 assert(auto_driver(evo1,"79787776") &&
        std::strcmp(auto_driver(evo1,"79787776"),"evo868")==0);
 assert(decode(evo1,"79787776","evo868")==PureDecodeStatus::Ok);
 assert(has(out,"total_m3",1.798));
 assert(has(out,"consumption_at_set_date_m3",1.225));
 assert(decode(evo2,"21031894","evo868")==PureDecodeStatus::Ok);
 assert(has(out,"total_m3",34.134));
 assert(has(out,"consumption_at_set_date_m3",25.296));
 assert(auto_driver(superwarm,"12345678") &&
        std::strcmp(auto_driver(superwarm,"12345678"),"supercom587")==0);
 assert(decode(superwarm,"12345678","supercom587")==PureDecodeStatus::Ok);
 assert(has(out,"total_m3",5.548));
 assert(decode(supercold,"11111111","supercom587")==PureDecodeStatus::Ok);
 assert(has(out,"total_m3",4.989));
 assert(decode(evo1,"79787777","evo868")!=PureDecodeStatus::Ok);
 assert(decode(superwarm,"12345679","supercom587")!=PureDecodeStatus::Ok);
 auto bad=evo1;bad[14]=0x25;
 assert(decode(bad,"79787776","evo868")!=PureDecodeStatus::Ok);
 bad=evo1;bad[0]++;
 assert(decode(bad,"79787776","evo868")!=PureDecodeStatus::Ok);
 bad=evo1;bad[8]=0x51;
 assert(decode(bad,"79787776","evo868")!=PureDecodeStatus::Ok);
 bad=evo1;bad[16]=0x6E;
 assert(decode(bad,"79787776","evo868")!=PureDecodeStatus::Ok);
 bad=superwarm;bad[14]=0x25;
 assert(decode(bad,"12345678","supercom587")!=PureDecodeStatus::Ok);
 bad=superwarm;bad[17]=0xAF; // bad BCD digit
 assert(decode(bad,"12345678","supercom587")!=PureDecodeStatus::Ok);
 bad=superwarm;bad.pop_back();bad[0]=uint8_t(bad.size()-1);
 assert(decode(bad,"12345678","supercom587")!=PureDecodeStatus::Ok);
 assert(out.value_count==0);
}
