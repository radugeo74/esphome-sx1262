// Upstream XMQ QWater V2 and Hydrodigit fixtures, configured-only.
#include "components/wmbus_native_sx126x/xmq_batch2_plain_dispatch.h"
#include "components/wmbus_native_sx126x/xmq_auto_registry.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <vector>
static std::vector<uint8_t> hex(const char*s) {
 std::vector<uint8_t> v;int hi=-1;
 for(;*s;++s){if(*s=='_'||*s==' ')continue;
 const int n=(*s>='0'&&*s<='9')?*s-'0':
             (*s>='a'&&*s<='f')?*s-'a'+10:
             (*s>='A'&&*s<='F')?*s-'A'+10:-1;
 assert(n>=0);if(hi<0)hi=n;else{
  v.push_back(static_cast<uint8_t>((hi<<4)|n));hi=-1;}}
 assert(hi<0);return v;
}
static bool has(const xmq_portable::DecodedPayload &out,
                const char *field,double expected) {
 for(size_t i=0;i<out.value_count;++i)
  if(out.values[i].name && std::strcmp(out.values[i].name,field)==0 &&
     std::fabs(out.values[i].value-expected)<0.00001)return true;
 return false;
}
int main() {
 using namespace xmq_portable;
 const auto q0=hex("3b4493444836351218067ac70000200c13911900004c1391170000426cbf2ccc081391170000c2086cbf2c02bb560000326cffff046d1e02de21fed0");
 const auto q4=hex("394493449400231218077ad30000200c13536712004c1307920500426cBf2ccc081373621200c2086cde2B02BB560000326cffff046d3714c32c");
 const auto q5=hex("394493441445141316067A31000020_0C13671605004C1348160500426CDF2CCC081348160500C2086CDF2C02BB560000326CFFFF046D3414E121");
 const auto q6=hex("394493449068171316077A0B000020_0C13358612004C1307851200426CDF2CCC081307851200C2086CDF2C02BB560000326CFFFF046D3014E121");
 const auto q9=hex("394493447426491317077ADD0000200C13975710004C1330970700426CFF2CCC081387471000C2086C1E3B02BB560000326CFFFF046D1E0F113C");
 const auto q13=hex("39449344411410601A067ADB000020_0C13780000004C1300000000426CFFFFCC081335000000C2086C3F3502BB560000326CFFFF046D14173136");
 const auto q14=hex("39449344893111601a077a580000200c13200300004c1300000000426cffffcc081334000000c2086c3f3502BB560000326cffff046d13173136");
 const auto q15=hex("39449344153119611D067A870000200C13370000004C1300000000426CFFFFCC081300000000C2086CFFFF02BB560000326CFFFF046D320D5E31");
 const auto h0=hex("4E44B4098686868613077AF0004005_2F2F0C1366380000046D27287E2A0F150E00000000C10000D10000E60000FD00000C01002F0100410100540100680100890000A00000B30000002F2F2F2F2F2F");
 const auto h2=hex("4644B4092143658713077A9C000000_0C1364390400_046D212F1635_0F152A0F000000440F00C00F00511000D51000B20B00180C007C0C00E60C00560D00D10D00400E00C60E0000");
 const auto h3=hex("4944B4092243658713077A7F000000_0C1363020400_046D242C1236_0F950A24042507000000A405006E0700850900CA0B004A0E00FFFFFFFFFFFF020000020000250000B3010095030000");
 const auto h4=hex("2144B4099163742315077A400000000C1399999999046D092A30340F050B01000000");
 const auto h5=hex("4C44B4096120120317077AB90000000C1330000000046D132E3E360F8F000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000");
 CalcValue values[16]{};CalendarDate dates[2]{};Capture caps[12]{};
 DecodedPayload out{values,16,0,dates,2,0};
 const auto decode=[&](const std::vector<uint8_t>&frame,
                       const char *id,const char *driver){
  return batch2_plain::decode_configured(frame.data(),frame.size(),
                                         id,id,driver,caps,12,out);
 };
 const auto automatic=[&](const std::vector<uint8_t>&frame,const char *id){
  return batch2_plain::select_configured_driver(
   frame.data(),frame.size(),id,id,"auto",
   wmbus_pure::generated::signatures,wmbus_pure::generated::signature_count);
 };
 assert(automatic(q0,"12353648") &&
        std::strcmp(automatic(q0,"12353648"),"qwaterv2")==0);
 assert(automatic(h4,"23746391") &&
        std::strcmp(automatic(h4,"23746391"),"hydrodigit")==0);
 assert(decode(q0,"12353648","qwaterv2")==PureDecodeStatus::Ok);
 assert(has(out,"total_m3",1.991));
 assert(has(out,"target_year_m3",1.791));
 assert(has(out,"target_m3",1.791));
 assert(decode(q4,"12230094","qwaterv2")==PureDecodeStatus::Ok);
 assert(has(out,"total_m3",126.753));
 assert(has(out,"target_year_m3",59.207));
 assert(has(out,"target_m3",126.273));
 assert(decode(q5,"13144514","qwaterv2")==PureDecodeStatus::Ok);
 assert(has(out,"total_m3",51.667));
 assert(has(out,"target_year_m3",51.648));
 assert(has(out,"target_m3",51.648));
 assert(decode(q6,"13176890","qwaterv2")==PureDecodeStatus::Ok);
 assert(has(out,"total_m3",128.635));
 assert(has(out,"target_year_m3",128.507));
 assert(has(out,"target_m3",128.507));
 assert(decode(q9,"13492674","qwaterv2")==PureDecodeStatus::Ok);
 assert(has(out,"total_m3",105.797));
 assert(has(out,"target_year_m3",79.73));
 assert(has(out,"target_m3",104.787));
 assert(decode(q13,"60101441","qwaterv2")==PureDecodeStatus::Ok);
 assert(has(out,"total_m3",0.078));
 assert(has(out,"target_year_m3",0));
 assert(has(out,"target_m3",0.035));
 assert(decode(q14,"60113189","qwaterv2")==PureDecodeStatus::Ok);
 assert(has(out,"total_m3",0.32));
 assert(has(out,"target_year_m3",0));
 assert(has(out,"target_m3",0.034));
 assert(decode(q15,"61193115","qwaterv2")==PureDecodeStatus::Ok);
 assert(has(out,"total_m3",0.037));
 assert(has(out,"target_year_m3",0));
 assert(has(out,"target_m3",0));
 assert(decode(h2,"87654321","hydrodigit")==PureDecodeStatus::Ok);
 assert(has(out,"total_m3",43.964));
 assert(decode(h3,"87654322","hydrodigit")==PureDecodeStatus::Ok);
 assert(has(out,"total_m3",40.263));
 assert(decode(h4,"23746391","hydrodigit")==PureDecodeStatus::Ok);
 assert(has(out,"total_m3",99999.999));
 assert(decode(h5,"03122061","hydrodigit")==PureDecodeStatus::Ok);
 assert(has(out,"total_m3",0.03));
 // Ciphertext and post-decrypted XMQ sample are not Mode-0 on-air RF.
 assert(decode(h0,"86868686","hydrodigit")!=PureDecodeStatus::Ok);
 assert(out.value_count==0);
 auto bad=q0;bad[14]=0x25;
 assert(decode(bad,"12353648","qwaterv2")!=PureDecodeStatus::Ok);
 assert(out.value_count==0);
 bad=h4;bad[14]=0x25;
 assert(decode(bad,"23746391","hydrodigit")!=PureDecodeStatus::Ok);
 bad=q0;bad[16]=0x14;
 assert(decode(bad,"12353648","qwaterv2")!=PureDecodeStatus::Ok);
 bad=q0;bad[0]++;
 assert(decode(bad,"12353648","qwaterv2")!=PureDecodeStatus::Ok);
 bad=h4;bad[16]=0x6e;
 assert(decode(bad,"23746391","hydrodigit")!=PureDecodeStatus::Ok);
 bad=h4;bad[8]=0x13;bad[9]=0x04;
 assert(decode(bad,"23746391","hydrodigit")!=PureDecodeStatus::Ok);
 assert(decode(q0,"12353647","qwaterv2")!=PureDecodeStatus::Ok);
 assert(decode(h4,"23746390","hydrodigit")!=PureDecodeStatus::Ok);
}
