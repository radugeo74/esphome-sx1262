// Native configured-only no-key receiver regressions for public samples.
// Generated original application payloads, no private telegrams or keys.
#include "components/wmbus_native_sx126x/xmq_batch2_plain_dispatch.h"
#include "components/wmbus_native_sx126x/xmq_auto_registry.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <vector>
static std::vector<uint8_t> unhex(const char *s){
 std::vector<uint8_t> v;int high=-1;
 for(;*s;++s){if(*s=='_'||*s==' ')continue;
 const int n=*s>='0'&&*s<='9'?*s-'0':
             *s>='a'&&*s<='f'?*s-'a'+10:
             *s>='A'&&*s<='F'?*s-'A'+10:-1;
 assert(n>=0);if(high<0)high=n;
 else{v.push_back(uint8_t(high*16+n));high=-1;}}
 assert(high<0);return v;
}
static bool has(const xmq_portable::DecodedPayload &out,
                const char *name,double expected){
 for(size_t i=0;i<out.value_count;++i)
 if(out.values[i].name && std::strcmp(out.values[i].name,name)==0 &&
    std::fabs(out.values[i].value-expected)<0.001)return true;
 return false;
}
int main(){
 using namespace xmq_portable;
 const auto e1=unhex("1844961557890080501B7aea00012002656f0802fd46610e0f");
 const auto e2=unhex("1e44961557890080501B7aff0010252f2f0265680802fd46630e0f2f2f2f2f");
 const auto e3=unhex("2b44961558890080501B7aff0010252f2f_02656808_02fd46630e_02fb1a0700_027c03324f430001_0f2f2f2f2f");
 const auto r1=unhex("54443330465903000B327A2B0000402F2F04FD3A946709008240FD3A600002FD0F9500818040FD3A0084C040FD3A8838000042FD3A28008201FD3A8C05C101FD3A7F8202FD3A3804066D35122EFB2B0002FD46D00C");
 const auto r2=unhex("54443330465903000B327A2B0400402F2F04FD3A946709008240FD3A600002FD0F9500818040FD3A0184C040FD3A8838000042FD3A28008201FD3A8C05C101FD3A088202FD3A3804066D35122EFB2B0002FD46D00C");
 CalcValue values[24]{};CalendarDate dates[2]{};Capture caps[12]{};
 DecodedPayload out{values,24,0,dates,2,0};
 const auto parse=[&](const std::vector<uint8_t>&f,const char*id,const char*driver){
   return batch2_plain::decode_configured(f.data(),f.size(),id,id,driver,caps,12,out);
 };
 const auto selected=[&](const std::vector<uint8_t>&f,const char*id){
 return batch2_plain::select_configured_driver(
    f.data(),f.size(),id,id,"auto",
    wmbus_pure::generated::signatures,
    wmbus_pure::generated::signature_count);
 };
 assert(selected(e1,"80008957") && std::strcmp(selected(e1,"80008957"),"elvsense")==0);
 assert(parse(e1,"80008957","elvsense")==PureDecodeStatus::Ok);
 assert(has(out,"temperature_c",21.59)&&has(out,"battery_v",3.681));
 assert(selected(e1,"80008958")==nullptr);
 assert(parse(e1,"80008958","elvsense")!=PureDecodeStatus::Ok);
 assert(!selected(r1,"00035947"));
 for(const auto *ptr:{&r1,&r2}){
 assert(selected(*ptr,"00035946") &&
        std::strcmp(selected(*ptr,"00035946"),"lansenrp")==0);
 assert(parse(*ptr,"00035946","lansenrp")==PureDecodeStatus::Ok);
 assert(has(out,"total_routed_messages_counter",616340));
 assert(has(out,"used_router_slots_counter",96));
 assert(has(out,"battery_v",3.28));
 }
 assert(parse(r1,"00035947","lansenrp")!=PureDecodeStatus::Ok);
 // XMQ is NOKEY, but both elvsense examples have Mode-5 CF;
 // a post-decrypted application must never be mistaken for RF clear.
 for(const auto *ptr:{&e2,&e3}){
 assert(parse(*ptr,ptr==&e2?"80008957":"80008958","elvsense")
        !=PureDecodeStatus::Ok);
 assert(out.value_count==0);
 }
 auto bad=e1;bad[14]=0x25;
 assert(parse(bad,"80008957","elvsense")!=PureDecodeStatus::Ok);
 bad=e1;bad[18]=0xFF;
 assert(parse(bad,"80008957","elvsense")!=PureDecodeStatus::Ok);
 bad=e1;bad[0]++;
 assert(parse(bad,"80008957","elvsense")!=PureDecodeStatus::Ok);
 bad=r1;bad[18]^=1;
 assert(parse(bad,"00035946","lansenrp")!=PureDecodeStatus::Ok);
 bad=r1;bad[40]=2;
 assert(parse(bad,"00035946","lansenrp")!=PureDecodeStatus::Ok);
 bad=r1;bad.resize(81);bad[0]=uint8_t(bad.size()-1);
 assert(parse(bad,"00035946","lansenrp")!=PureDecodeStatus::Ok);
 assert(out.value_count==0);
}
