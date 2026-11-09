// Official public upstream Ei6500 and Apator E-ITN40 fixtures.
// Diagnostic Ei6500 readings must never be published as fire safety state.
#include "components/wmbus_native_sx126x/xmq_batch2_plain_dispatch.h"
#include "components/wmbus_native_sx126x/xmq_auto_registry.h"
#include "components/wmbus_native_sx126x/xmq_plain_text_fields.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <vector>
static std::vector<uint8_t> hex(const char*s){
 std::vector<uint8_t> v;int high=-1;
 for(;*s;++s){if(*s=='_'||*s==' ')continue;
 int n=*s>='0'&&*s<='9'?*s-'0':*s>='a'&&*s<='f'?*s-'a'+10:
       *s>='A'&&*s<='F'?*s-'A'+10:-1;
 assert(n>=0);if(high<0)high=n;else{v.push_back(static_cast<uint8_t>((high<<4)|n));high=-1;}}
 assert(high<0);return v;
}
static bool has(const xmq_portable::DecodedPayload&o,const char*n,double value) {
 for(size_t i=0;i<o.value_count;++i)
  if(o.values[i].name && std::strcmp(o.values[i].name,n)==0 &&
     std::fabs(o.values[i].value-value)<1e-6)return true;
 return false;
}
int main() {
 using namespace xmq_portable;
 const auto e0=hex("58442515747209010C1A7A8B0000000BFD0F070101046D2A06D82502FD17000082206CD825426CD0238440FF2C000F11008250FD61000082506C01018260FD6100008360FD3100000082606C01018270FD61000082706C0101");
 const auto e1=hex("58442515747209010C1A7A8D0000000BFD0F070101046D2E06D82502FD17000082206CD825426CD0238440FF2C000F11008250FD61000082506C01018260FD6100008360FD3100000082606C01018270FD61020082706CD825");
 const auto e2=hex("58442515747209010C1A7A900000000BFD0F070101046D3406D82502FD17000082206CD825426CD0238440FF2C020F11008250FD61010082506CD8258260FD6100008360FD3100000082606C01018270FD61020082706CD825");
 const auto e3=hex("58442515747209010C1A7A940000000BFD0F070101046D0007D82502FD17000082206CD825426CD0238440FF2C420F11008250FD61010082506CD8258260FD6101008360FD3101000082606CD8258270FD61020082706CD825");
 const auto e4=hex("58442515747209010C1A7A8B0000000BFD0F070101046D2A06D82502FD17000082206CD825426CD0238440FF2C1b0F11008250FD61000082506C01018260FD6100008360FD3100000082606C01018270FD61000082706C0101");
 const auto e5=hex("58442515747209010C1A7A8B0000000BFD0F070101046D2A06D82502FD17000082206CD825426CD0238440FF2C1c0F11008250FD61000082506C01018260FD6100008360FD3100000082606C01018270FD61000082706C0101");
 const auto a0=hex("40440106139901000908A00000A10081016B013F00A2004E00000000000000000000000000000018001E0007007800100002002F01C700000000002D3006000000");
 const auto a1=hex("40440106073238000D08A0000081000000F90007001000000000000000000000000000000000000000000000000000000000001700000000000000A83204000000");
 const auto a2=hex("40440106139901000908A00000A10000006D0100000000000000000000000000001A002F000E0006000000000000000000000000005D003F01C7002C3305000000");
 CalcValue values[24]{};CalendarDate dates[2]{};Capture caps[12]{};
 DecodedPayload out{values,24,0,dates,2,0};
 const auto decode=[&](const std::vector<uint8_t>&frame,const char*id,const char*driver){
   return batch2_plain::decode_configured(frame.data(),frame.size(),id,id,driver,caps,12,out);
 };
 const auto auto_select=[&](const std::vector<uint8_t>&frame,const char*id){
   return batch2_plain::select_configured_driver(frame.data(),frame.size(),id,id,"auto",
     wmbus_pure::generated::signatures,wmbus_pure::generated::signature_count);
 };
 assert(auto_select(e0,"01097274") &&
        std::strcmp(auto_select(e0,"01097274"),"ei6500")==0);
 assert(auto_select(a0,"00019913") &&
        std::strcmp(auto_select(a0,"00019913"),"apatoreitn40")==0);
 assert(decode(e0,"01097274","ei6500")==PureDecodeStatus::Ok);
 assert(out.value_count==4);
 assert(has(out,"alarm_counter",0));
 assert(has(out,"removed_counter",0));
 assert(has(out,"test_button_counter",0));
 assert(has(out,"dust_level_counter",0));
 assert(decode(e1,"01097274","ei6500")==PureDecodeStatus::Ok);
 assert(out.value_count==4);
 assert(has(out,"alarm_counter",0));
 assert(has(out,"removed_counter",0));
 assert(has(out,"test_button_counter",2));
 assert(has(out,"dust_level_counter",0));
 assert(decode(e2,"01097274","ei6500")==PureDecodeStatus::Ok);
 assert(out.value_count==4);
 assert(has(out,"alarm_counter",1));
 assert(has(out,"removed_counter",0));
 assert(has(out,"test_button_counter",2));
 assert(has(out,"dust_level_counter",2));
 assert(decode(e3,"01097274","ei6500")==PureDecodeStatus::Ok);
 assert(out.value_count==4);
 assert(has(out,"alarm_counter",1));
 assert(has(out,"removed_counter",1));
 assert(has(out,"test_button_counter",2));
 assert(has(out,"dust_level_counter",2));
 assert(decode(e4,"01097274","ei6500")==PureDecodeStatus::Ok);
 assert(out.value_count==4);
 assert(has(out,"alarm_counter",0));
 assert(has(out,"removed_counter",0));
 assert(has(out,"test_button_counter",0));
 assert(has(out,"dust_level_counter",27));
 assert(decode(e5,"01097274","ei6500")==PureDecodeStatus::Ok);
 assert(out.value_count==4);
 assert(has(out,"alarm_counter",0));
 assert(has(out,"removed_counter",0));
 assert(has(out,"test_button_counter",0));
 assert(has(out,"dust_level_counter",28));
 assert(decode(a0,"00019913","apatoreitn40")==PureDecodeStatus::Ok);
 assert(out.value_count==8);
 assert(has(out,"current_hca",303));
 assert(has(out,"previous_hca",199));
 assert(has(out,"previous_2y_hca",0));
 assert(has(out,"previous_3y_hca",0));
 assert(has(out,"current_month_hca",63));
 assert(has(out,"previous_month_hca",162));
 assert(has(out,"temp_radiator_c",24.0625));
 assert(has(out,"temp_room_c",22.6875));
 assert(decode(a1,"00383207","apatoreitn40")==PureDecodeStatus::Ok);
 assert(out.value_count==8);
 assert(has(out,"current_hca",23));
 assert(has(out,"previous_hca",0));
 assert(has(out,"previous_2y_hca",0));
 assert(has(out,"previous_3y_hca",0));
 assert(has(out,"current_month_hca",7));
 assert(has(out,"previous_month_hca",16));
 assert(has(out,"temp_radiator_c",0));
 assert(has(out,"temp_room_c",15.5625));
 assert(decode(a2,"00019913","apatoreitn40")==PureDecodeStatus::Ok);
 assert(out.value_count==8);
 assert(has(out,"current_hca",0));
 assert(has(out,"previous_hca",93));
 assert(has(out,"previous_2y_hca",319));
 assert(has(out,"previous_3y_hca",199));
 assert(has(out,"current_month_hca",0));
 assert(has(out,"previous_month_hca",0));
 assert(has(out,"temp_radiator_c",0));
 assert(has(out,"temp_room_c",22.8125));
 // A valid Ei6500 diagnostic frame says nothing about current smoke status.
 for(const auto &v:{e0,e1,e2,e3,e4,e5}) {
   assert(text_fields::decode(v.data(),v.size(),"ei6500").count==0);
 }
 // Tampering, wrong CI/MVT/keyless AES revisions and invalid frames fail.
 assert(decode(e0,"01097275","ei6500")!=PureDecodeStatus::Ok);
 assert(decode(a0,"00019914","apatoreitn40")!=PureDecodeStatus::Ok);
 auto bad=e0;bad[14]=0x25;
 assert(decode(bad,"01097274","ei6500")!=PureDecodeStatus::Ok);
 bad=e0;bad[52]=0x62;
 assert(decode(bad,"01097274","ei6500")!=PureDecodeStatus::Ok);
 bad=e0;bad[30]=0x01;
 assert(decode(bad,"01097274","ei6500")!=PureDecodeStatus::Ok);
 bad=e0;bad[0]++;
 assert(decode(bad,"01097274","ei6500")!=PureDecodeStatus::Ok);
 bad=a0;bad[8]=0x0A;
 assert(decode(bad,"00019913","apatoreitn40")!=PureDecodeStatus::Ok);
 bad=a1;bad[8]=0x0E;
 assert(decode(bad,"00383207","apatoreitn40")!=PureDecodeStatus::Ok);
 bad=a0;bad[9]=0x07;
 assert(decode(bad,"00019913","apatoreitn40")!=PureDecodeStatus::Ok);
 bad=a0;bad[64]=0x01;
 assert(decode(bad,"00019913","apatoreitn40")!=PureDecodeStatus::Ok);
 bad=a0;bad[0]++;
 assert(decode(bad,"00019913","apatoreitn40")!=PureDecodeStatus::Ok);
 assert(out.value_count==0);
}
