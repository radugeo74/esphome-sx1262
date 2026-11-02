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
  const uint8_t heat_frame[] = {
    0x44,0x11,0x22, // simplified prefix, used only for span boundary checks
    0x02,0x03, // application bytes before the manufacturer marker
    0x0F,0x9F,0x29,0x2D,0x00,0x50,0x24,0x04,0x00,
    0x11,0xBD,0x08,0x38,0x09,0x04,0x00,0x00,0x00,
    0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,
    0x00,0x00,0x03,0x14,0x0E, 0xaa // suffix
  };
  const DriverTables heat{
    generated_ixml::fhkvdataiii::rules, generated_ixml::fhkvdataiii::count,
    generated_ixml::fhkvdataiii::capture_keys, 6,
    generated_ixml::fhkvdataiii::field_keys, 2,
    generated_ixml::fhkvdataiii::calculations,
    generated_ixml::fhkvdataiii::calculation_count,
    generated_ixml::fhkvdataiii::date_calculations,
    generated_ixml::fhkvdataiii::date_calculation_count};
  CalendarDate heat_dates[2]{};
  DecodedPayload heat_result{values,16,0,heat_dates,2,0};
  assert(manufacturer_payload(heat_frame,sizeof(heat_frame),3,1,2,payload)==PayloadStatus::Ok);
  assert(payload.data==heat_frame+5 && payload.data[0]==0x0F);
  assert(decode_manufacturer_frame(heat,heat_frame,sizeof(heat_frame),3,1,2,
                                   captures,8,heat_result)==DriverStatus::Ok);
  assert(heat_dates[0].year==2021 && heat_dates[0].month==2 && heat_dates[0].day==5);
  assert(heat_dates[1].year==2020 && heat_dates[1].month==12 && heat_dates[1].day==31);
  assert(manufacturer_payload(heat_frame,sizeof(heat_frame),3,1,-1,payload)==PayloadStatus::InvalidBounds);
  assert(manufacturer_payload(heat_frame,sizeof(heat_frame),3,1,0,payload)==PayloadStatus::InvalidBounds);
  assert(manufacturer_payload(heat_frame,sizeof(heat_frame),3,1,99,payload)==PayloadStatus::InvalidBounds);
  assert(decode_manufacturer_frame(heat,heat_frame,sizeof(heat_frame),3,1,-1,
                                   captures,8,heat_result)==DriverStatus::BadArguments);
  assert(heat_result.value_count==0 && heat_result.date_count==0);
  return 0;
}
