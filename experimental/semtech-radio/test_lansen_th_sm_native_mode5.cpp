// End-to-end native keyed dispatch, four original public applications
// independently re-encrypted with a synthetic AES-128 key. No RF validation.
#include "components/wmbus_native_sx126x/xmq_batch2_secure_dispatch.h"
#include <mbedtls/aes.h>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
static std::vector<uint8_t> hex(const char *s){
 std::vector<uint8_t> v;int hi=-1;
 for(;*s;++s){
   if(*s=='_'||*s==' ')continue;
   int n=*s>='0'&&*s<='9'?*s-'0':*s>='a'&&*s<='f'?*s-'a'+10:
         *s>='A'&&*s<='F'?*s-'A'+10:-1;
   assert(n>=0);
   if(hi<0)hi=n;
   else{v.push_back(uint8_t(hi*16+n));hi=-1;}
 }
 assert(hi<0);return v;
}
static std::vector<uint8_t> encrypt(const std::vector<uint8_t>&plain,
                                     const uint8_t key[16]){
 assert(plain.size()==47 && plain[15]==0x2f && plain[16]==0x2f);
 std::vector<uint8_t> out=plain;
 uint8_t iv[16]{};std::memcpy(iv,plain.data()+2,8);
 std::memset(iv+8,plain[11],8);
 mbedtls_aes_context ctx;mbedtls_aes_init(&ctx);
 assert(mbedtls_aes_setkey_enc(&ctx,key,128)==0);
 assert(mbedtls_aes_crypt_cbc(&ctx,MBEDTLS_AES_ENCRYPT,32,iv,
                             plain.data()+15,out.data()+15)==0);
 mbedtls_aes_free(&ctx);return out;
}
static bool field(const xmq_portable::DecodedPayload &out,const char *name,double val){
 for(size_t i=0;i<out.value_count;++i)if(out.values[i].name &&
     std::strcmp(out.values[i].name,name)==0 &&
     std::fabs(out.values[i].value-val)<0.00001)return true;
 return false;
}
int main(){
 const char *samples[]={
 "2e44333003020100071b7a634820252f2f0265840842658308820165950802fb1aae0142fb1aae018201fb1aa9012f",
 "2E44333041000600091B7AA70020252F2F_0265DBF94265FC04820165610901FB1B2C41FB1B238101FB1B290223BB00",
 "2E44333004020100031A7AC40020052F2F_02FD971D000004FD084C02000004FD3A467500002F2F2F2F2F2F2F2F2F2F",
 "2E44333004020100031A7ADE0020052F2F_02FD971D040004FD086502000004FD3A010000002F2F2F2F2F2F2F2F2F2F"};
 const char *ids[]={"00010203","00060041","00010204","00010204"};
 const uint8_t key[16]={1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
 uint8_t wrong[16]={1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,17};
 xmq_portable::CalcValue values[24]{};
 xmq_portable::CalendarDate dates[2]{};
 xmq_portable::DecodedPayload out{values,24,0,dates,2,0};
 using S=xmq_portable::batch2_secure::Status;
 for(size_t i=0;i<4;++i){
   auto clear=hex(samples[i]);
   const auto cipher=encrypt(clear,key);
   const char *driver=i<2?"lansenth":"lansensm";
   const auto decode=[&](const std::vector<uint8_t>&bytes,
                         const char*id,const uint8_t*k){
     return xmq_portable::batch2_secure::decode_configured(
       bytes.data(),bytes.size(),id,driver,k,out);
   };
   assert(decode(cipher,ids[i],key)==S::Ok);
   if(i==0){assert(field(out,"current_temperature_c",21.8));
            assert(field(out,"current_relative_humidity_rh",43));}
   if(i==1){assert(field(out,"current_temperature_c",-15.73));
            assert(field(out,"current_relative_humidity_rh",44));}
   if(i==2){assert(field(out,"smoke_detected",0));
            assert(field(out,"minutes_since_last_manual_test_counter",30022));}
   if(i==3){assert(field(out,"smoke_detected",1));
            assert(field(out,"minutes_since_last_manual_test_counter",1));}
   assert(decode(clear,ids[i],key)!=S::Ok && out.value_count==0);
   assert(decode(cipher,ids[i],wrong)!=S::Ok && out.value_count==0);
   assert(decode(cipher,"99999999",key)!=S::Ok && out.value_count==0);
   auto bad=cipher;bad[8]=0x44;
   assert(decode(bad,ids[i],key)!=S::Ok && out.value_count==0);
   bad=cipher;bad[14]=0;
   assert(decode(bad,ids[i],key)!=S::Ok && out.value_count==0);
   bad=cipher;bad.pop_back();bad[0]=uint8_t(bad.size()-1);
   assert(decode(bad,ids[i],key)!=S::Ok && out.value_count==0);
 }
}
