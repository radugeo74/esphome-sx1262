// Original upstream RFM-AMB / Sensostar fixtures under strict Mode-0 admission.
#include "components/wmbus_native_sx126x/xmq_batch2_plain_dispatch.h"
#include "components/wmbus_native_sx126x/xmq_auto_registry.h"
#include "components/wmbus_native_sx126x/xmq_plain_text_fields.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <vector>
static std::vector<uint8_t> from_hex(const char*s){
 std::vector<uint8_t> v;int hi=-1;
 for(;*s;++s){if(*s=='_'||*s==' ')continue;
 int n=*s>='0'&&*s<='9'?*s-'0':*s>='a'&&*s<='f'?*s-'a'+10:
       *s>='A'&&*s<='F'?*s-'A'+10:-1;
 assert(n>=0);if(hi<0)hi=n;
 else{v.push_back(uint8_t(hi*16+n));hi=-1;}}
 assert(hi<0);return v;
}
static bool has(const xmq_portable::DecodedPayload&out,
                const char*name,double value){
 for(size_t i=0;i<out.value_count;++i)
 if(out.values[i].name && std::strcmp(out.values[i].name,name)==0 &&
    std::fabs(out.values[i].value-value)<1e-6)return true;
 return false;
}
int main() {
 using namespace xmq_portable;
 const auto r1=from_hex("5744b40988227711101b7ab20800000265a00842658f088201659f08226589081265a0086265510852652b0902fb1aba0142fb1ab0018201fb1abd0122fb1aa90112fb1aba0162fb1aa60152fb1af501066d3b3bb36b2a00");
 const auto r2=from_hex("5e44b40958956923101b7a7f085005816afb5f6f40ec742610b93c109973edd8c098f505d5dfcfd53dfd72708178e4b81436cb753c6dff5094c48c26607c66419628d424ce41f38f9bd927757d82fa43396aa59a77c694acadab776460d472");
 const auto wired=from_hex("68B3B36808007257004820c51400046c100000047839803801040600000000041300000000042B00000000142B00000000043B00000000143B00000000025B1400025f15000261daff02235c00046d2c2ddc24440600000000441300000000426c000001fd171003fd0c05000084200600000000c420060000000084300600000000c430060000000084401300000000c44013000000008480401300000000c48040130000000084c0401300000000c4c0401300000000a216");
 const auto s1=from_hex("a444c5146025750200047ac20000202f2f046d2e26c62a040643160000041310f0050001fd1700426cbf2c4406570e00008401061f160000840206f6150000840306f5150000840406f3150000840506ea150000840606bf1500008407065214000084080692120000840906c5100000840a06570e0000840b06ca0b0000840c06da090000840d06ca080000840e06c8080000840f06c608000003fd0c05010002fd0b2111");
 const auto s2=from_hex("4944C5144404752100047AC10000202F2F046D142D073404068847000001FD170004138E8A4000043BF7FFFFFF042B00000000025B3E00025F360002612F0303FD0C05010002FD0B2011");
 const auto encrypted=from_hex("5944C5149178454500047A100040252F2F_046D11334F3904FB0DE70C000001FD170004130DAB0600043B00000000042B00000000025B1B00025F1B000261F3FF2F2F2F2F2F2F2F2F2F2F2F2F2F2F2F03FD0C05000002FD0B1011");
 CalcValue values[24]{};CalendarDate dates[2]{};Capture captures[12]{};
 DecodedPayload out{values,24,0,dates,2,0};
 const auto parse=[&](const std::vector<uint8_t>&frame,
                      const char*id,const char*driver){
   return batch2_plain::decode_configured(frame.data(),frame.size(),
             id,id,driver,captures,12,out);
 };
 const auto automatic=[&](const std::vector<uint8_t>&frame,const char*id){
   return batch2_plain::select_configured_driver(frame.data(),frame.size(),
      id,id,"auto",wmbus_pure::generated::signatures,
      wmbus_pure::generated::signature_count);
 };
 assert(automatic(r1,"11772288") && 
        std::strcmp(automatic(r1,"11772288"),"rfmamb")==0);
 assert(parse(r1,"11772288","rfmamb")==PureDecodeStatus::Ok);
 assert(out.value_count==6);
 const auto rfm_text=text_fields::decode(r1.data(),r1.size(),"rfmamb");
 assert(rfm_text.count==1 && std::strcmp(rfm_text.fields[0].name,"status")==0);
 assert(std::strcmp(rfm_text.fields[0].text,"MODULE_REMOVED")==0);
 assert(has(out,"current_temperature_c",22.08));
 assert(has(out,"average_temperature_1h_c",21.91));
 assert(has(out,"average_temperature_24h_c",22.07));
 assert(has(out,"current_relative_humidity_rh",44.2));
 assert(has(out,"average_relative_humidity_1h_rh",43.2));
 assert(has(out,"average_relative_humidity_24h_rh",44.5));
 assert(automatic(s1,"02752560") &&
        std::strcmp(automatic(s1,"02752560"),"sensostar")==0);
 assert(parse(s1,"02752560","sensostar")==PureDecodeStatus::Ok);
 assert(has(out,"total_kwh",5699));
 assert(has(out,"total_water_m3",389.136));
 assert(has(out,"target_kwh",3671));
 const auto ss_text=text_fields::decode(s1.data(),s1.size(),"sensostar");
 assert(ss_text.count==1 && std::strcmp(ss_text.fields[0].name,"status")==0);
 assert(std::strcmp(ss_text.fields[0].text,"OK")==0);
 assert(parse(s2,"21750444","sensostar")==PureDecodeStatus::Ok);
 assert(has(out,"total_kwh",18312));
 assert(has(out,"total_water_m3",4229.774));
 assert(parse(r2,"23699558","rfmamb")!=PureDecodeStatus::Ok);
 assert(out.value_count==0); // actual AES-5 RF cannot enter Mode0 path
 assert(text_fields::decode(r2.data(),r2.size(),"rfmamb").count==0);
 assert(parse(encrypted,"45457891","sensostar")!=PureDecodeStatus::Ok);
 assert(out.value_count==0); // Mode-5 supplied-clear XMQ suffix is NOT clear RF
 assert(parse(wired,"20480057","sensostar")!=PureDecodeStatus::Ok);
 assert(out.value_count==0); // no wired M-Bus through SX126x
 assert(automatic(r1,"11772289")==nullptr);
 assert(parse(r1,"11772289","rfmamb")!=PureDecodeStatus::Ok);
 assert(parse(s1,"02752561","sensostar")!=PureDecodeStatus::Ok);
 auto invalid=r1;invalid[13]=0x50;invalid[14]=0x05;
 assert(parse(invalid,"11772288","rfmamb")!=PureDecodeStatus::Ok);
 invalid=r1;invalid.pop_back();invalid[0]=uint8_t(invalid.size()-1);
 assert(parse(invalid,"11772288","rfmamb")!=PureDecodeStatus::Ok);
 invalid=s1;invalid[13]=0x40;invalid[14]=0x25;
 assert(parse(invalid,"02752560","sensostar")!=PureDecodeStatus::Ok);
 invalid=s2;invalid[8]=0x11;
 assert(parse(invalid,"21750444","sensostar")!=PureDecodeStatus::Ok);
}
