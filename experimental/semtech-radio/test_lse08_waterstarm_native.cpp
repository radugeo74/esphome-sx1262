// Source-anchored LSE/01/08 and DWZ Waterstarm clear-TPL native fixtures.
#include "components/wmbus_native_sx126x/xmq_batch2_plain_dispatch.h"
#include "components/wmbus_native_sx126x/xmq_auto_registry.h"
#include "components/wmbus_native_sx126x/xmq_plain_text_fields.h"
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
static bool has(const xmq_portable::DecodedPayload &out,
                const char *name,double value){
 for(size_t i=0;i<out.value_count;++i)
 if(out.values[i].name && std::strcmp(out.values[i].name,name)==0 &&
    std::fabs(out.values[i].value-value)<1e-5)return true;
 return false;
}
int main() {
 using namespace xmq_portable;
 const auto l0=from_hex("294465324185990401087a0080000082046c7f018b046e210300046d1a0e6f0202fdac7e012301fd0c01");
 const auto w0=from_hex("3944FA122162092002067A3600202567C94D48D00DC47B11213E23383DB51968A705AAFA60C60E263D50CD259D7C9A03FD0C08000002FD0B0011");
 const auto w1=from_hex("3944FA122162092002067A3604202567C94D48D00DC47B11213E23383DB51968A705AAFA60C60E263D50CD259D7C9A03FD0C08000002FD0B0011");
 const auto w2=from_hex("3944FA122162992202067A360420252F2F_046D282A9E2704136A00000002FD17400004933C000000002F2F2F2F2F2F03FD0C08000002FD0B0011");
 const auto w3=from_hex("2E44FA129999551100077A070020252F2F_046D0F28C22404139540000002FD17000001FD481D2F2F2F2F2F2F2F2F2F");
 const auto w4=from_hex("9644FA126606052000067A1E000020_046D3B2ED729041340D8000002FD17000001FD481D426CBF2C4413026C000084011348D20000C40113F3CB0000840213DCC40000C40213B8B60000840313849B0000C403138B8C0000840413E3800000C4041337770000840513026C0000C40513D65F00008406134F560000C40613604700008407139D370000C407137F3300008408135B2C0000");
 const auto w5=from_hex("9644FA126051062000077A78000020_046D392DD7290413901A000002FD17000001FD481D426CBF2C4413D312000084011399190000C40113841800008402130C180000C40213EC16000084031395150000C40313E3140000840413BD130000C404134C130000840513D3120000C4051322120000840613AF110000C4061397100000840713D00F0000C40713890E0000840813980C0000");
 const auto w6=from_hex("a144c5142966495070078c20d07a0d0090257db8bb9938a1ff7c4b9a4492f4ea5f278b725057eee6837c17397e9605f3a448a338bd8407a2a5846632c334dff577f427f054f55e68a00fa5c85ccbc808ccdd2bb537a83234a50392968f00c1d7473455f9fc4c88fb195ca1712325da8c6aa7fdb7c4b77b9b84a4a0ccac4f586775fa66007dbdc2c615b69247401e9b9a863ac5a5873484b1b7d0178198e88f701e53"); // original keyed ELL, always excluded
 const auto w7=from_hex("A144C5146587395170078C20947AD10090252F2F_046D862D4A3104131300000001FD1700426C3F3C44130000000044933C0000000084011300000000C401130100000084021300000000C402130000000084031300000000C4031300000000840413FFFFFFFFC40413FFFFFFFF840513FFFFFFFFC40513FFFFFFFF840613FFFFFFFFC40613FFFFFFFF840713FFFFFFFFC40713FFFFFFFF840813FFFFFFFF2F2F2F2F");
 CalcValue values[16]{};CalendarDate dates[2]{};Capture caps[12]{};
 DecodedPayload out{values,16,0,dates,2,0};
 const auto decode=[&](const std::vector<uint8_t>&f,const char*id,const char*driver){
 return batch2_plain::decode_configured(f.data(),f.size(),id,id,driver,caps,12,out);
 };
 const auto automatic=[&](const std::vector<uint8_t>&f,const char*id){
 return batch2_plain::select_configured_driver(f.data(),f.size(),id,id,"auto",
   wmbus_pure::generated::signatures,wmbus_pure::generated::signature_count);
 };
 assert(automatic(l0,"04998541") &&
        std::strcmp(automatic(l0,"04998541"),"lse_08")==0);
 assert(automatic(w4,"20050666") &&
        std::strcmp(automatic(w4,"20050666"),"waterstarm")==0);
 assert(decode(l0,"04998541","lse_08")==PureDecodeStatus::Ok);
 assert(has(out,"consumption_at_set_date_hca",321));
 assert(has(out,"duration_since_readout_h",2.489166666666667));
 assert(has(out,"duration_since_readout_s",8961));
 assert(has(out,"tpl_status_raw",128));
 assert(out.value_count==4);
 const auto text_status=text_fields::decode(l0.data(),l0.size(),"lse_08");
 assert(text_status.count==4);
 const auto text_eq=[&](const char*name,const char*expected){
   for(size_t i=0;i<text_status.count;++i)
     if(std::strcmp(text_status.fields[i].name,name)==0 &&
        std::strcmp(text_status.fields[i].text,expected)==0)return true;
   return false;
 };
 assert(text_eq("status","TPL_STS_128"));
 assert(text_eq("set_date","2003-01-31"));
 assert(text_eq("device_datetime","2003-02-15 14:26"));
 assert(text_eq("model_version","01"));
 assert(decode(w4,"20050666","waterstarm")==PureDecodeStatus::Ok);
 assert(has(out,"total_m3",55.36));
 assert(has(out,"consumption_at_set_date_m3",27.65));
 assert(decode(w5,"20065160","waterstarm")==PureDecodeStatus::Ok);
 assert(has(out,"total_m3",6.8));
 assert(has(out,"consumption_at_set_date_m3",4.819));
 assert(decode(w0,"20096221","waterstarm")!=PureDecodeStatus::Ok);
 assert(out.value_count==0);
 assert(decode(w1,"20096221","waterstarm")!=PureDecodeStatus::Ok);
 assert(out.value_count==0);
 assert(decode(w2,"22996221","waterstarm")!=PureDecodeStatus::Ok);
 assert(out.value_count==0);
 assert(decode(w3,"11559999","waterstarm")!=PureDecodeStatus::Ok);
 assert(out.value_count==0);
 assert(decode(w6,"50496629","waterstarm")!=PureDecodeStatus::Ok);
 assert(out.value_count==0);
 assert(decode(w7,"51398765","waterstarm")!=PureDecodeStatus::Ok);
 assert(out.value_count==0);
 assert(decode(l0,"04998542","lse_08")!=PureDecodeStatus::Ok);
 assert(decode(w4,"20050667","waterstarm")!=PureDecodeStatus::Ok);
 auto bad=l0;bad[14]=0x25;
 assert(decode(bad,"04998541","lse_08")!=PureDecodeStatus::Ok);
 bad=l0;bad[23]=0xff;
 assert(decode(bad,"04998541","lse_08")!=PureDecodeStatus::Ok);
 bad=l0;bad[18]=0; // invalid set_date day
 assert(decode(bad,"04998541","lse_08")!=PureDecodeStatus::Ok);
 assert(out.value_count==0);
 bad=l0;bad[32]=0x03; // missing duration-since-readout record
 assert(decode(bad,"04998541","lse_08")!=PureDecodeStatus::Ok);
 assert(text_fields::decode(bad.data(),bad.size(),"lse_08").count==0);
 bad=l0;bad[35]=0; // missing combinable FutureValue semantics
 assert(decode(bad,"04998541","lse_08")!=PureDecodeStatus::Ok);
 bad=l0;bad[12]=0; // never map unexpected TPL status to OK
 assert(decode(bad,"04998541","lse_08")!=PureDecodeStatus::Ok);
 bad=l0;bad[41]=0x0F; // model version record changed
 assert(decode(bad,"04998541","lse_08")==PureDecodeStatus::Ok);
 const auto changed_version=text_fields::decode(bad.data(),bad.size(),"lse_08");
 bool matched=false;
 for(size_t i=0;i<changed_version.count;++i)
   if(std::strcmp(changed_version.fields[i].name,"model_version")==0)
     matched=(std::strcmp(changed_version.fields[i].text,"0F")==0);
 assert(matched);
 bad=w4;bad[14]=0x25;
 assert(decode(bad,"20050666","waterstarm")!=PureDecodeStatus::Ok);
 bad=w4;bad[22]=0x15;
 assert(decode(bad,"20050666","waterstarm")!=PureDecodeStatus::Ok);
 bad=w4;bad[0]++;
 assert(decode(bad,"20050666","waterstarm")!=PureDecodeStatus::Ok);
 assert(out.value_count==0);
}
