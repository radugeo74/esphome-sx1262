// Original upstream MAD Microclima and ABB B23 clear-TPL Mode0 source
// fixtures; bad MVT, ID, security, layout and BCD fail closed.
#include "components/wmbus_native_sx126x/xmq_batch2_plain_dispatch.h"
#include "components/wmbus_native_sx126x/xmq_auto_registry.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <vector>
static std::vector<uint8_t> hex(const char*s) {
 std::vector<uint8_t> b;int h=-1;
 for(;*s;++s){if(*s=='_'||*s==' ')continue;
 int x=*s>='0'&&*s<='9'?*s-'0':*s>='a'&&*s<='f'?*s-'a'+10:
       *s>='A'&&*s<='F'?*s-'A'+10:-1;
 assert(x>=0);if(h<0)h=x;else{
 b.push_back(static_cast<uint8_t>((h<<4)|x));h=-1;}}
 assert(h<0);return b;
}
static bool has(const xmq_portable::DecodedPayload &o,const char*name,double v) {
 for(size_t i=0;i<o.value_count;++i)
 if(o.values[i].name&&std::strcmp(o.values[i].name,name)==0 &&
    std::fabs(o.values[i].value-v)<1e-5)return true;
 return false;
}
int main() {
 using namespace xmq_portable;
 const auto m0=hex("494424343124579300047a5a0000202f2f046d2720b62c04060d07000001fd170004130a8c0400043b00000000042b00000000025b1500025f15000261d0ff03fd0c05000002fd0b1011");
 const auto m1=hex("A44424348630579300047ADD0000202F2F046D0721B62C04064708000004135DB0030001FD1700426C9F2C4406C6040000C40106C0070000C4020637070000C4030611070000C404060B070000C405060B070000C406060B070000C407060B070000C40806A5060000C40906F7050000C40A067A050000C40B060F050000C40C06C6040000C40D063F040000C40E06BB030000C40F06A502000003FD0C05000002FD0B1111");
 const auto b0=hex("844442040011223320027A3E000020_0E840017495200000004FFA0150000000004FFA1150000000004FFA2150000000004FFA3150000000007FFA600000000000000000007FFA700000000000000000007FFA800000000000000000007FFA90000000000000000000DFD8E0007302E38322E31420DFFAA000B3030312D313131203332421F");
 CalcValue vals[16]{};CalendarDate dates[2]{};Capture caps[12]{};
 DecodedPayload out{vals,16,0,dates,2,0};
 const auto decode=[&](const std::vector<uint8_t>&f,const char*id,const char*name){
 return batch2_plain::decode_configured(f.data(),f.size(),id,id,name,caps,12,out);
 };
 const auto automatic=[&](const std::vector<uint8_t>&f,const char*id){
 return batch2_plain::select_configured_driver(f.data(),f.size(),id,id,"auto",
   wmbus_pure::generated::signatures,wmbus_pure::generated::signature_count);
 };
 assert(automatic(m0,"93572431") &&
        std::strcmp(automatic(m0,"93572431"),"microclima")==0);
 assert(automatic(b0,"33221100") &&
        std::strcmp(automatic(b0,"33221100"),"abbb23")==0);
 assert(decode(m0,"93572431","microclima")==PureDecodeStatus::Ok);
 assert(out.value_count==4);
 assert(has(out,"total_energy_consumption_kwh",1805));
 assert(has(out,"total_volume_m3",297.994));
 assert(has(out,"flow_temperature_c",21));
 assert(has(out,"return_temperature_c",21));
 assert(decode(m1,"93573086","microclima")==PureDecodeStatus::Ok);
 assert(out.value_count==2);
 assert(has(out,"total_energy_consumption_kwh",2119));
 assert(has(out,"total_volume_m3",241.757));
 assert(decode(b0,"33221100","abbb23")==PureDecodeStatus::Ok);
 assert(out.value_count==1);
 assert(has(out,"total_energy_consumption_kwh",5249.17));
 assert(decode(m0,"93572432","microclima")!=PureDecodeStatus::Ok);
 assert(decode(b0,"33221101","abbb23")!=PureDecodeStatus::Ok);
 auto bad=m0;bad[14]=0x25; // encrypted Mode5
 assert(decode(bad,"93572431","microclima")!=PureDecodeStatus::Ok);
 bad=m0;bad[24]=0x07;
 assert(decode(bad,"93572431","microclima")!=PureDecodeStatus::Ok);
 bad=m0;bad[0]++;
 assert(decode(bad,"93572431","microclima")!=PureDecodeStatus::Ok);
 bad=m0;bad[9]=0x05;
 assert(decode(bad,"93572431","microclima")!=PureDecodeStatus::Ok);
 bad=m0;bad[35]=0xFA; // current volume corrupted
 assert(decode(bad,"93572431","microclima")!=PureDecodeStatus::Ok ||
        !has(out,"total_volume_m3",297.994));
 bad=b0;bad[14]=0x25;
 assert(decode(bad,"33221100","abbb23")!=PureDecodeStatus::Ok);
 bad=b0;bad[9]=0x07;
 assert(decode(bad,"33221100","abbb23")!=PureDecodeStatus::Ok);
 bad=b0;bad[17]=0x01;
 assert(decode(bad,"33221100","abbb23")!=PureDecodeStatus::Ok);
 bad=b0;bad[20]=0xFA; // invalid BCD
 assert(decode(bad,"33221100","abbb23")!=PureDecodeStatus::Ok);
 bad=b0;bad[0]++;
 assert(decode(bad,"33221100","abbb23")!=PureDecodeStatus::Ok);
 assert(out.value_count==0);
}
