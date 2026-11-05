// Host-only executable for comparing existing clear-application decoders
// to the POST-DECRYPTED portions of original upstream XMQ test fixtures.
// Does not accept/reinterpret a raw encrypted RF frame; no sensor publishing.
#include "components/wmbus_native_sx126x/minomess_clear_prefix.h"
#include "components/wmbus_native_sx126x/kamwater_clear_records.h"
#include "components/wmbus_native_sx126x/kamwater_tpl79_probe.h"
#include "host_only/apator162_ixml_fields.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <cmath>
static int hex_digit(char c) {
  if(c>='0' && c<='9') return c-'0';
  if(c>='a' && c<='f') return 10+c-'a';
  if(c>='A' && c<='F') return 10+c-'A';
  return -1;
}
int main(int argc,char **argv) {
  if(argc!=3) return 2;
  const char *src=argv[2];
  const size_t n=std::strlen(src);
  if(n==0 || n%2 || n>1024) return 2;
  std::vector<uint8_t> bytes;
  for(size_t i=0;i<n;i+=2) {
    const int hi=hex_digit(src[i]),lo=hex_digit(src[i+1]);
    if(hi<0 || lo<0) return 2;
    bytes.push_back(static_cast<uint8_t>((hi<<4)|lo));
  }
  if(std::strcmp(argv[1],"minomess")==0) {
    xmq_portable::minomess::Reading reading{};
    const auto result=xmq_portable::minomess::decode_clear_application(
        bytes.data(),bytes.size(),reading);
    if(result!=xmq_portable::minomess::Status::Ok) return 3;
    std::printf("total_m3=%.9f\n",reading.total_m3);
    if(reading.has_target)
      std::printf("target_m3=%.9f\n",reading.target_m3);
    else std::puts("target_m3=null");
    if(reading.has_meter_date)
      std::printf("meter_date=%04d-%02d-%02d\n",
          reading.meter_date.year,reading.meter_date.month,
          reading.meter_date.day);
    if(reading.has_target_date)
      std::printf("target_date=%04d-%02d-%02d\n",
          reading.target_date.year,reading.target_date.month,
          reading.target_date.day);
    return 0;
  }
  if(std::strcmp(argv[1],"apator162")==0) {
    xmq_portable::apator162::Reading reading{};
    const auto result=xmq_portable::apator162::decode_clear_manufacturer(
        bytes.data(),bytes.size(),reading);
    if(result!=xmq_portable::apator162::Status::Ok) return 3;
    std::printf("total_m3=%.9f\n",reading.total_m3);
    return 0;
  }
  if(std::strcmp(argv[1],"kamwater_compact")==0) {
    uint8_t expanded[160]{};
    xmq_portable::kamwater::tpl79::Reading compact{};
    if(xmq_portable::kamwater::tpl79::verify(
        bytes.data(),bytes.size(),expanded,sizeof(expanded),compact)
        !=xmq_portable::kamwater::tpl79::Status::Ok) return 3;
    xmq_portable::kamwater::Reading reading{};
    if(xmq_portable::kamwater::decode_clear_records(
        expanded,compact.expanded_size,reading)
        !=xmq_portable::kamwater::Status::Ok) return 3;
    std::printf("total_m3=%.9f\n",reading.total_m3);
    if(reading.has_target)
      std::printf("target_m3=%.9f\n",reading.target_m3);
    else std::puts("target_m3=null");
    return 0;
  }
  if(std::strcmp(argv[1],"kamwater")==0) {
    xmq_portable::kamwater::Reading reading{};
    const auto result=xmq_portable::kamwater::decode_clear_records(
        bytes.data(),bytes.size(),reading);
    if(result!=xmq_portable::kamwater::Status::Ok) return 3;
    std::printf("total_m3=%.9f\n",reading.total_m3);
    if(reading.has_target)
      std::printf("target_m3=%.9f\n",reading.target_m3);
    else std::puts("target_m3=null");
    return 0;
  }
  return 2;
}
