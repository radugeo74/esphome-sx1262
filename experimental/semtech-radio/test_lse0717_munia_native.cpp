// Official upstream LSE/18 water and WEP Munia clear-only fixtures.
// Test values are public upstream examples, not user's real meter IDs.
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
 const int n=(*s>='0'&&*s<='9')?*s-'0':
             (*s>='a'&&*s<='f')?*s-'a'+10:
             (*s>='A'&&*s<='F')?*s-'A'+10:-1;
 assert(n>=0);if(hi<0)hi=n;
 else {out.push_back(static_cast<uint8_t>((hi<<4)|n));hi=-1;}}
 assert(hi<0);return out;
}
static bool has(const xmq_portable::DecodedPayload&out,const char*field,double expected){
 for(size_t i=0;i<out.value_count;++i)
 if(out.values[i].name && std::strcmp(out.values[i].name,field)==0 &&
    std::fabs(out.values[i].value-expected)<0.00001)return true;
 return false;
}
int main() {
 using namespace xmq_portable;
 const auto l0=hex("244465329933961318067AE1000000_8C04130070000082046CBE2B01FD0C11046D010CA22C");
 const auto l1=hex("2A4465329933961318067AD8800000_8C04130070000082046CBE2B01FD0C11046D1800A12C02FDAC7E9B2E");
 const auto l2=hex("2D4465329933961318067ADA000000_0C13567100004C1300000000426CFFFF02BB560000326CFFFF046D2307A12C");
 const auto l3=hex("2d44653223369909d8077a80000000046d130aed2B0c13233332004c1351762700426cdf2c326cffff02BB560000");
 const auto m0=hex("2E44B05C82340100021B7A460000002F2F0A6601020AFB1A570602FD971D00002F2F2F2F2F2F2F2F2F2F2F2F2F2F2F");
 const auto m1=hex("2E44B05C11012200041B7A2B0000002F2F0A6617020AFB1A100602FD971D00002F2F2F2F2F2F2F2F2F2F2F2F2F2F2F");
 CalcValue vals[16]{};CalendarDate dates[2]{};Capture caps[12]{};
 DecodedPayload out{vals,16,0,dates,2,0};
 const auto decode=[&](const std::vector<uint8_t>&f,const char*id,const char*driver){
    return batch2_plain::decode_configured(f.data(),f.size(),id,id,driver,caps,12,out);
 };
 const auto automatic=[&](const std::vector<uint8_t>&f,const char*id){
    return batch2_plain::select_configured_driver(f.data(),f.size(),id,id,"auto",
      wmbus_pure::generated::signatures,wmbus_pure::generated::signature_count);
 };
 assert(automatic(l0,"13963399") &&
        std::strcmp(automatic(l0,"13963399"),"lse_07_17")==0);
 assert(automatic(m0,"00013482") &&
        std::strcmp(automatic(m0,"00013482"),"munia")==0);
 // The upstream last LSE example carries 0xD8, absent in detect.mvt:
 // explicit parser proves values; AUTO does NOT guess that revision.
 assert(automatic(l3,"09993623")==nullptr);
 assert(decode(l0,"13963399","lse_07_17")==PureDecodeStatus::Ok);
 assert(out.value_count==1);
 assert(has(out,"consumption_at_set_date_8_m3",7));
 assert(decode(l1,"13963399","lse_07_17")==PureDecodeStatus::Ok);
 assert(out.value_count==1);
 assert(has(out,"consumption_at_set_date_8_m3",7));
 assert(decode(l2,"13963399","lse_07_17")==PureDecodeStatus::Ok);
 assert(out.value_count==2);
 assert(has(out,"total_m3",7.156));
 assert(has(out,"due_m3",0));
 assert(decode(l3,"09993623","lse_07_17")==PureDecodeStatus::Ok);
 assert(out.value_count==2);
 assert(has(out,"total_m3",323.323));
 assert(has(out,"due_m3",277.651));
 assert(decode(m0,"00013482","munia")==PureDecodeStatus::Ok);
 assert(out.value_count==2);
 assert(has(out,"current_temperature_c",20.1));
 assert(has(out,"current_relative_humidity_rh",65.7));
 const auto stat0=text_fields::decode(m0.data(),m0.size(),"munia");
 assert(stat0.count==1 && std::strcmp(stat0.fields[0].text,"OK")==0);
 assert(decode(m1,"00220111","munia")==PureDecodeStatus::Ok);
 assert(out.value_count==2);
 assert(has(out,"current_temperature_c",21.7));
 assert(has(out,"current_relative_humidity_rh",61));
 const auto stat1=text_fields::decode(m1.data(),m1.size(),"munia");
 assert(stat1.count==1 && std::strcmp(stat1.fields[0].text,"OK")==0);
 assert(decode(l0,"13963398","lse_07_17")!=PureDecodeStatus::Ok);
 assert(decode(m0,"00013483","munia")!=PureDecodeStatus::Ok);
 auto bad=l2;bad[14]=0x25;
 assert(decode(bad,"13963399","lse_07_17")!=PureDecodeStatus::Ok);
 bad=l2;bad[16]=0x14;
 assert(decode(bad,"13963399","lse_07_17")!=PureDecodeStatus::Ok);
 bad=l2;bad[0]++;
 assert(decode(bad,"13963399","lse_07_17")!=PureDecodeStatus::Ok);
 bad=l0;bad[17]=0x14;
 assert(decode(bad,"13963399","lse_07_17")!=PureDecodeStatus::Ok);
 bad=l3;bad[8]=0x18; // keep medium but wrong observed revision/profile
 assert(decode(bad,"09993623","lse_07_17")!=PureDecodeStatus::Ok);
 bad=m0;bad[14]=0x25;
 assert(decode(bad,"00013482","munia")!=PureDecodeStatus::Ok);
 bad=m0;bad[22]=0x13; // wrong humidity VIF family
 assert(decode(bad,"00013482","munia")!=PureDecodeStatus::Ok);
 bad=m0;bad[25]=0xFA; // invalid BCD nibble
 assert(decode(bad,"00013482","munia")!=PureDecodeStatus::Ok);
 bad=m0;bad[30]=0x01; // nonzero status flags, never label OK
 assert(decode(bad,"00013482","munia")!=PureDecodeStatus::Ok);
 bad=m0;bad[46]=0x01; // non-padding tail
 assert(decode(bad,"00013482","munia")!=PureDecodeStatus::Ok);
 bad=m0;bad[0]++;
 assert(decode(bad,"00013482","munia")!=PureDecodeStatus::Ok);
 assert(out.value_count==0);
}
