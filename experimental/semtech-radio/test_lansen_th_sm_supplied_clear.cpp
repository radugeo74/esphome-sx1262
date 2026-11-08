// Public already-clear upstream samples, NOT live clear on-air RF.
#include "components/wmbus_native_sx126x/lansenth_clear_application.h"
#include "components/wmbus_native_sx126x/lansensm_clear_application.h"
#include <cassert>
#include <cmath>
#include <vector>
#include <cstdint>
static std::vector<uint8_t> hex(const char*s){
std::vector<uint8_t>v;int hi=-1;
for(;*s;++s){int n=*s>='0'&&*s<='9'?*s-'0':*s>='a'&&*s<='f'?*s-'a'+10:*s>='A'&&*s<='F'?*s-'A'+10:-1;
assert(n>=0);if(hi<0)hi=n;else{v.push_back(uint8_t(hi*16+n));hi=-1;}}
assert(hi<0);return v;}
int main(){using namespace xmq_portable;
auto th=hex("2f2f0265840842658308820165950802fb1aae0142fb1aae018201fb1aa9012f");
lansenth_clear::Reading t{};
assert(lansenth_clear::decode(th.data(),th.size(),t)==lansenth_clear::Status::Ok);
assert(std::fabs(t.temperature_c-21.8)<0.001);
assert(std::fabs(t.temperature_1h_c-21.79)<0.001);
assert(std::fabs(t.temperature_24h_c-21.97)<0.001);
assert(std::fabs(t.humidity_rh-43)<0.001);
assert(std::fabs(t.humidity_1h_rh-43)<0.001);
assert(std::fabs(t.humidity_24h_rh-42.5)<0.001);
const auto th2=hex("2f2f0265dbf94265fc04820165610901fb1b2c41fb1b238101fb1b290223bb00");
assert(lansenth_clear::decode(th2.data(),th2.size(),t)==lansenth_clear::Status::Ok);
assert(std::fabs(t.temperature_c+15.73)<0.001);
assert(std::fabs(t.temperature_1h_c-12.76)<0.001);
assert(std::fabs(t.temperature_24h_c-24.01)<0.001);
assert(std::fabs(t.humidity_rh-44)<0.001);
assert(std::fabs(t.humidity_1h_rh-35)<0.001);
assert(std::fabs(t.humidity_24h_rh-41)<0.001);
auto bad=th;bad[0]=0;
assert(lansenth_clear::decode(bad.data(),bad.size(),t)!=lansenth_clear::Status::Ok);
assert(t.temperature_c==0);
bad=th;bad.pop_back();bad.resize(10);
assert(lansenth_clear::decode(bad.data(),bad.size(),t)!=lansenth_clear::Status::Ok);
auto smoke_ok=hex("2f2f02fd971d000004fd084c02000004fd3a467500002f2f2f2f2f2f2f2f2f2f");
auto smoke_on=hex("2f2f02fd971d040004fd086502000004fd3a010000002f2f2f2f2f2f2f2f2f2f");
lansensm_clear::Reading s{};
assert(lansensm_clear::decode(smoke_ok.data(),smoke_ok.size(),s)==lansensm_clear::Status::Ok);
assert(!s.smoke&&s.message_counter==588&&s.minutes_since_test==30022);
assert(lansensm_clear::decode(smoke_on.data(),smoke_on.size(),s)==lansensm_clear::Status::Ok);
assert(s.smoke&&s.message_counter==613&&s.minutes_since_test==1);
bad=smoke_on;bad[6]=0x80;
assert(lansensm_clear::decode(bad.data(),bad.size(),s)!=lansensm_clear::Status::Ok);
assert(!s.smoke&&s.message_counter==0);
bad=smoke_on;bad.resize(12);
assert(lansensm_clear::decode(bad.data(),bad.size(),s)!=lansensm_clear::Status::Ok);
}
