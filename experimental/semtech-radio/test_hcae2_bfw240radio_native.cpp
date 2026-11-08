// Official upstream HCA E2 and BFW 240 radio fixtures; no real meter IDs.
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
static bool has(const xmq_portable::DecodedPayload&out,const char*name,double v){
 for(size_t i=0;i<out.value_count;++i)
 if(out.values[i].name && std::strcmp(out.values[i].name,name)==0 &&
    std::fabs(out.values[i].value-v)<0.00001)return true;
 return false;
}
int main() {
 using namespace xmq_portable;
 const auto h0=from_hex("2D46C5141731425431087A0A0B00002F2F046D24344535036E000000426C5E34436E000000317F1A346D00335B34");
 const auto h1=from_hex("C946C5141731425431087AD00B00002F2F046D14374535036E000000426C5E34436E00000082016C2F3283016E000000C2016E000082026E0000C2026E000082036E0000C2036E000082046E0000C2046E000082056E0000C2056E000082066E0000C2066E000082076E0000C2076E000082086E0000C2086E000082096E0000C2096E0000820A6E0000C20A6E0000820B6E0000C20B6E0000820C6E0000C20C6E0000820D6E0000C20D6E0000820E6E0000C20E6E0000820F6E0000C20F6E0000317F1A346D00335B34");
 const auto h2=from_hex("2F46C5141731425431087A0A0B00002F2F046D28344535036E000000426C5E34436E000000317F1A346D00335B340186");
 const auto h3=from_hex("CD46C5141731425431087A0A0B00002F2F046D2A344535036E000000426C5E34436E00000082016C2F3283016E000000C2016E000082026E0000C2026E000082036E0000C2036E000082046E0000C2046E000082056E0000C2056E000082066E0000C2066E000082076E0000C2076E000082086E0000C2086E000082096E15420000C2096E0000820A6E0000C20A6E0000820B6E0000C20B6E0000820C6E0000C20C6E0000820D6E0000C20D6E0000820E6E0000C20E6E0000820F6E0000C20F6E0000317F1A346D00335B34BB6D");
 const auto h4=from_hex("CE44C5146029412331087A6E00C0252F2F046D042C4635036EF20000426C3F3C436E75010082016C2F3283016EAD0000C2016E220082026E0C00C2026E1A0082036E0500C2036E000082046E0000C2046E000082056E0000C2056E000082066E0000C2066E000082076E0000C2076E000082086E0000C2086E000082096E0000C2096E0C00820A6E0D00C20A6E2200820B6E2900C20B6E1700820C6EB1FEC20C6E4300820D6E1E00C20D6E2800820E6E0200C20E6E2100820F6E1900C20F6E0700317F00346D002000002F2F2F2F2F");
 const auto b0=from_hex("3644D7088877700002087ADBC000002F2F9E1F03C10388152A00000000000000000000000000000204000404000EE2020AC1321D280221");
 const auto b1=from_hex("3644D7088877700002087A8BC000002F2F011F03C1038D152A0000000000000000000000000200040400040E00E20A23C11D238D010321");
 const auto b2=from_hex("3644D7087670700002087A9CC000002F2F6E1F000000000B36000000000000000000000000000000000000000000000000000000260221");
 const auto b3=from_hex("3644D7087670700002087A27C000002F2F011F000000000B36000000000000000000000000000000000000000000000000000000010321");
 const auto b4=from_hex("3644D7084774700002087A80C000002F2F6F1F01440100147000000000000000000000000000000000000000000037009B4410AC260221");
 const auto b5=from_hex("3644D7084774700002087AE1C000002F2F481F0144010014700000000000000000000000000000000000000000379B0144AC0100040321");
 CalcValue values[16]{};CalendarDate dates[2]{};Capture caps[12]{};
 DecodedPayload out{values,16,0,dates,2,0};
 const auto decode=[&](const std::vector<uint8_t>&f,const char*id,const char*driver){
 return batch2_plain::decode_configured(f.data(),f.size(),id,id,driver,caps,12,out);
 };
 const auto automatic=[&](const std::vector<uint8_t>&f,const char*id){
 return batch2_plain::select_configured_driver(f.data(),f.size(),id,id,"auto",
   wmbus_pure::generated::signatures,wmbus_pure::generated::signature_count);
 };
 assert(automatic(h0,"54423117") &&
        std::strcmp(automatic(h0,"54423117"),"hcae2")==0);
 assert(automatic(b0,"00707788") &&
        std::strcmp(automatic(b0,"00707788"),"bfw240radio")==0);
 assert(decode(h0,"54423117","hcae2")==PureDecodeStatus::Ok);
 assert(has(out,"consumption_hca",0));
 assert(has(out,"target_hca",0));
 assert(decode(h1,"54423117","hcae2")==PureDecodeStatus::Ok);
 assert(has(out,"consumption_hca",0));
 assert(has(out,"target_hca",0));
 assert(decode(h2,"54423117","hcae2")!=PureDecodeStatus::Ok); // raw upstream C1 CRC-bearing not eligible
 assert(decode(h3,"54423117","hcae2")!=PureDecodeStatus::Ok); // raw upstream C1 CRC-bearing not eligible
 // The original ActiveLong header is Mode 5 (CF=0x25C0):
 // its NOKEY XMQ application was ALREADY DECRYPTED upstream.
 // Never treat that upstream supplied-clear payload as on-air plaintext.
 assert(decode(h4,"23412960","hcae2")!=PureDecodeStatus::Ok);
 assert(out.value_count==0);
 auto synthetic_clear=h4;
 synthetic_clear[13]=0;
 synthetic_clear[14]=0; // host-only synthetic Mode 0, NOT an RF fixture
 assert(decode(synthetic_clear,"23412960","hcae2")==PureDecodeStatus::Ok);
 assert(has(out,"consumption_hca",242));
 assert(has(out,"target_hca",373));
 assert(decode(b0,"00707788","bfw240radio")==PureDecodeStatus::Ok);
 assert(has(out,"current_hca",904));
 assert(has(out,"prev_hca",961));
 assert(decode(b1,"00707788","bfw240radio")==PureDecodeStatus::Ok);
 assert(has(out,"current_hca",909));
 assert(has(out,"prev_hca",961));
 assert(decode(b2,"00707076","bfw240radio")==PureDecodeStatus::Ok);
 assert(has(out,"current_hca",0));
 assert(has(out,"prev_hca",0));
 assert(decode(b3,"00707076","bfw240radio")==PureDecodeStatus::Ok);
 assert(has(out,"current_hca",0));
 assert(has(out,"prev_hca",0));
 assert(decode(b4,"00707447","bfw240radio")==PureDecodeStatus::Ok);
 assert(has(out,"current_hca",256));
 assert(has(out,"prev_hca",324));
 assert(decode(b5,"00707447","bfw240radio")==PureDecodeStatus::Ok);
 assert(has(out,"current_hca",256));
 assert(has(out,"prev_hca",324));
 const auto hstatus=text_fields::decode(h0.data(),h0.size(),"hcae2");
 assert(hstatus.count==1 && std::strcmp(hstatus.fields[0].name,"status")==0);
 assert(std::strcmp(hstatus.fields[0].text,
   "ALARM OPEN_C PERMANENT_ERROR SENSOR_BREAK UNEXPECTED_RST")==0);
 assert(text_fields::decode(h4.data(),h4.size(),"hcae2").count==0);
 const auto hstatus_ok=text_fields::decode(
    synthetic_clear.data(),synthetic_clear.size(),"hcae2");
 assert(hstatus_ok.count==1 && std::strcmp(hstatus_ok.fields[0].text,"OK")==0);
 assert(decode(h0,"54423118","hcae2")!=PureDecodeStatus::Ok);
 assert(decode(b0,"00707789","bfw240radio")!=PureDecodeStatus::Ok);
 auto bad=h0;bad[14]=0x25;
 assert(decode(bad,"54423117","hcae2")!=PureDecodeStatus::Ok);
 bad=synthetic_clear;bad[8]=0x30;
 assert(decode(bad,"23412960","hcae2")!=PureDecodeStatus::Ok);
 bad=synthetic_clear;bad[23]=0xFF;
 assert(decode(bad,"23412960","hcae2")!=PureDecodeStatus::Ok);
 bad=b0;bad[14]=0x25;
 assert(decode(bad,"00707788","bfw240radio")!=PureDecodeStatus::Ok);
 bad=b0;bad[18]=0x20;
 assert(decode(bad,"00707788","bfw240radio")!=PureDecodeStatus::Ok);
 bad=b0;bad[52]=0x4A; // invalid BCD day
 assert(decode(bad,"00707788","bfw240radio")!=PureDecodeStatus::Ok);
 bad=b0;bad.pop_back();bad[0]=static_cast<uint8_t>(bad.size()-1);
 assert(decode(bad,"00707788","bfw240radio")!=PureDecodeStatus::Ok);
 assert(out.value_count==0);
}
