#include "components/wmbus_native_sx126x/xmq_plain_text_fields.h"
#include <cassert>
#include <cstring>
#include <vector>
static std::vector<uint8_t> parse(const char *src) {
  std::vector<uint8_t> v;int hi=-1;
  for(;*src;++src) {
    if(*src=='_')continue;
    int n=(*src>='0'&&*src<='9')?*src-'0':
        (*src>='A'&&*src<='F')?*src-'A'+10:-1;
    assert(n>=0);
    if(hi<0)hi=n;
    else {v.push_back(static_cast<uint8_t>((hi<<4)|n));hi=-1;}
  }
  assert(hi<0);return v;
}
static bool has(xmq_portable::text_fields::Result result,
                const char *key,const char *value) {
  for(size_t i=0;i<result.count;i++)
    if(std::strcmp(result.fields[i].name,key)==0 &&
       std::strcmp(result.fields[i].text,value)==0) return true;
  return false;
}
int main() {
  auto iperl=parse("1844AE4C4455223368077A55000000_041389E20100023B0000");
  auto fields=xmq_portable::text_fields::decode(iperl.data(),iperl.size(),"iperl");
  assert(has(fields,"status","OK"));
  iperl[12]=0x06;
  fields=xmq_portable::text_fields::decode(iperl.data(),iperl.size(),"iperl");
  assert(has(fields,"status","ERROR POWER_LOW"));
  iperl[12]=0x01;
  fields=xmq_portable::text_fields::decode(iperl.data(),iperl.size(),"iperl");
  assert(fields.count==0); // unsupported status cannot masquerade as OK
  auto water=parse("4E44A5116464646470077AED004005_2F2F01FD08300C13741100007C1300000000FC101300000000FC201300000000726C00000B3B00000002FD748713025A6800C4016D3B177F2ACC011300020000");
  fields=xmq_portable::text_fields::decode(water.data(),water.size(),"hydrus");
  assert(fields.count==0); // original RF mode 5, no decryption
  water[14]=0; // synthetic mode 0 to validate already-clear application
  fields=xmq_portable::text_fields::decode(water.data(),water.size(),"hydrus");
  assert(has(fields,"target_datetime","2019-10-31 23:59"));
  return 0;
}
