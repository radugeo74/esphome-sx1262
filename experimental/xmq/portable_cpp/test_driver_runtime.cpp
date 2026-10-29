#include "driver_runtime.h"
#include "generated_ixml.h"
#include <cassert>
#include <cmath>
#include <cstring>
using namespace xmq_portable;

static double field(const DecodedPayload &out, const char *name) {
  for (size_t i = 0; i < out.value_count; ++i)
    if (std::strcmp(out.values[i].name, name) == 0) return out.values[i].value;
  assert(false && "missing calculated field");
  return 0;
}
int main() {
  const DriverTables water{
    generated_ixml::mkradio4::rules, generated_ixml::mkradio4::count,
    generated_ixml::mkradio4::capture_keys, 2,
    generated_ixml::mkradio4::field_keys, 2,
    generated_ixml::mkradio4::calculations, generated_ixml::mkradio4::calculation_count,
    nullptr, 0};
  const uint8_t water_payload[] = {0x06,0x70,0x29,0x01,0x00,0x60,0x17,
                                    0x03,0x00,0x04,0x00,0x03,0x00,0x00,0x00};
  Capture captures[12]{};
  CalcValue values[20]{};
  CalendarDate dates[2]{};
  DecodedPayload output{values,20,0,dates,2,0};
  assert(decode_driver_payload(water,water_payload,sizeof(water_payload),
                               captures,12,output)==DriverStatus::Ok);
  assert(std::fabs(field(output,"target")-0.1)<0.00001);
  assert(std::fabs(field(output,"total")-0.4)<0.00001);
  assert(output.date_count==0);
  const DriverTables heat{
    generated_ixml::fhkvdataiii::rules, generated_ixml::fhkvdataiii::count,
    generated_ixml::fhkvdataiii::capture_keys, 6,
    generated_ixml::fhkvdataiii::field_keys, 2,
    generated_ixml::fhkvdataiii::calculations, generated_ixml::fhkvdataiii::calculation_count,
    generated_ixml::fhkvdataiii::date_calculations,
    generated_ixml::fhkvdataiii::date_calculation_count};
  const uint8_t heat_payload[] = {0x0F,0x9F,0x29,0x2D,0x00,0x50,0x24,0x04,0x00,
    0x11,0xBD,0x08,0x38,0x09,0x04,0x00,0x00,0x00,
    0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,
    0x00,0x00,0x03,0x14,0x0E};
  assert(decode_driver_payload(heat,heat_payload,sizeof(heat_payload),
                               captures,12,output)==DriverStatus::Ok);
  assert(output.date_count==2);
  assert(dates[0].year==2021 && dates[0].month==2 && dates[0].day==5);
  assert(dates[1].year==2020 && dates[1].month==12 && dates[1].day==31);
  assert(field(output,"previous_day")==31);
  assert(field(output,"current_day")==5);
  assert(decode_driver_payload(heat,heat_payload,2,captures,12,output)
         ==DriverStatus::ParseError);
  assert(output.value_count==0 && output.date_count==0);
  assert(decode_driver_payload(heat,nullptr,2,captures,12,output)
         ==DriverStatus::BadArguments);
  return 0;
}
