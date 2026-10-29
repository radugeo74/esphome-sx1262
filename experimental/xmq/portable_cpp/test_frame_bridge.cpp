#include "frame_bridge.h"
#include "generated_ixml.h"
#include <cassert>
#include <cmath>
using namespace xmq_portable;
int main() {
  const DriverTables water{
    generated_ixml::mkradio4::rules, generated_ixml::mkradio4::count,
    generated_ixml::mkradio4::capture_keys, 2,
    generated_ixml::mkradio4::field_keys, 2,
    generated_ixml::mkradio4::calculations,
    generated_ixml::mkradio4::calculation_count, nullptr, 0};
  const uint8_t frame[] = {
    0x10,0x20,0x30,0x40,
    0x06,0x70,0x29,0x01,0x00,0x60,0x17,0x03,0x00,0x04,0x00,0x03,0x00,0x00,0x00,
    0xaa,0xbb};
  PayloadSpan payload{};
  assert(application_payload(frame,sizeof(frame),4,2,payload)==PayloadStatus::Ok);
  assert(payload.data==frame+4 && payload.length==sizeof(frame)-6);
  Capture captures[8]{};
  CalcValue values[16]{};
  DecodedPayload result{values,16,0,nullptr,0,0};
  assert(decode_parsed_frame(water,frame,sizeof(frame),4,2,captures,8,result)==DriverStatus::Ok);
  bool found_target=false, found_total=false;
  for(size_t i=0;i<result.value_count;++i) {
    if (std::strcmp(result.values[i].name,"target")==0)
      found_target=std::fabs(result.values[i].value-0.1)<1e-8;
    if (std::strcmp(result.values[i].name,"total")==0)
      found_total=std::fabs(result.values[i].value-0.4)<1e-8;
  }
  assert(found_target && found_total);
  assert(application_payload(frame,sizeof(frame),sizeof(frame)+1,0,payload)==PayloadStatus::InvalidBounds);
  assert(payload.data==nullptr && payload.length==0);
  assert(application_payload(frame,sizeof(frame),4,sizeof(frame),payload)==PayloadStatus::InvalidBounds);
  assert(application_payload(frame,sizeof(frame),4,sizeof(frame)-4,payload)==PayloadStatus::EmptyPayload);
  assert(decode_parsed_frame(water,frame,sizeof(frame),4,sizeof(frame),captures,8,result)==DriverStatus::BadArguments);
  assert(result.value_count==0);
  assert(application_payload(frame,sizeof(frame),0,0,payload)==PayloadStatus::Ok);
  return 0;
}
