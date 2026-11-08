#include "components/wmbus_native_sx126x/xmq_batch2_plain_dispatch.h"
#include "components/wmbus_native_sx126x/xmq_auto_registry.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <vector>
static std::vector<uint8_t> bytes(const char*s){
 std::vector<uint8_t> v;int half=-1;
 for(;*s;++s){if(*s=='_'||*s==' ')continue;
 const int digit=(*s>='0'&&*s<='9')?*s-'0':
                 (*s>='a'&&*s<='f')?*s-'a'+10:
                 (*s>='A'&&*s<='F')?*s-'A'+10:-1;
 assert(digit>=0);
 if(half<0)half=digit;else{v.push_back(uint8_t(half*16+digit));half=-1;}}
 assert(half<0);return v;
}
static bool has(const xmq_portable::DecodedPayload&out,const char*name,double value){
 for(size_t i=0;i<out.value_count;++i)
 if(out.values[i].name && std::strcmp(name,out.values[i].name)==0 &&
    std::fabs(out.values[i].value-value)<0.000001)return true;
 return false;
}
int main(){
 using namespace xmq_portable;
 const auto smoke=bytes("3744934450881248231A7A5C00002081027C034955230082026CFFFF81037C034C41230082036CFFFF02FD170000326CFFFF046D2514BC2B");
 const auto q1=bytes("314493441234567835087a740000200b6e2701004b6e450100426c5f2ccb086e790000c2086c7f21326cffff046d200b7422");
 const auto q2=bytes("314493449392919034087a520000200b6e9700004b6e700200426c9f2ccb086e970000c2086cbe26326cffff046d2d16a227");
 CalcValue val[24]{};CalendarDate dates[2]{};Capture captures[12]{};
 DecodedPayload out{val,24,0,dates,2,0};
 const auto parse=[&](const std::vector<uint8_t>&frame,const char*id,const char*driver){
 return batch2_plain::decode_configured(frame.data(),frame.size(),id,id,driver,
                                        captures,12,out);
 };
 const auto selected=[&](const std::vector<uint8_t>&frame,const char*id){
 return batch2_plain::select_configured_driver(frame.data(),frame.size(),id,id,
       "auto",wmbus_pure::generated::signatures,
       wmbus_pure::generated::signature_count);
 };
 assert(selected(smoke,"48128850") &&
        std::strcmp(selected(smoke,"48128850"),"qsmoke")==0);
 assert(parse(smoke,"48128850","qsmoke")==PureDecodeStatus::Ok);
 assert(has(out,"alarm_counter",0)&&has(out,"test_button_counter",0));
 assert(selected(q1,"78563412") &&
        std::strcmp(selected(q1,"78563412"),"qcaloric")==0);
 assert(parse(q1,"78563412","qcaloric")==PureDecodeStatus::Ok);
 assert(has(out,"current_consumption_hca",127));
 assert(has(out,"consumption_at_set_date_hca",145));
 assert(parse(q2,"90919293","qcaloric")==PureDecodeStatus::Ok);
 assert(has(out,"current_consumption_hca",97));
 assert(has(out,"consumption_at_set_date_hca",270));
 auto bad=smoke;bad[14]=0x25;
 assert(parse(bad,"48128850","qsmoke")!=PureDecodeStatus::Ok);
 bad=smoke;bad[0]++;
 assert(parse(bad,"48128850","qsmoke")!=PureDecodeStatus::Ok);
 assert(parse(smoke,"48128851","qsmoke")!=PureDecodeStatus::Ok);
 bad=q1;bad[14]=0x25;
 assert(parse(bad,"78563412","qcaloric")!=PureDecodeStatus::Ok);
 bad=q1;bad.resize(27);bad[0]=uint8_t(bad.size()-1);
 assert(parse(bad,"78563412","qcaloric")!=PureDecodeStatus::Ok);
 assert(out.value_count==0);
}
