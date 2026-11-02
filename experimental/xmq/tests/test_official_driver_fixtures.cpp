// Regression tests: original test blocks embedded in the two pinned XMQ driver definitions.
// No wmbusmeters legacy parser, no Telegram, no handleTelegram.
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include "../../../components/wmbus_meter/xmq_decode_catalog.h"

static bool near_value(const xmq_portable::DecodedPayload &d, const char *field, double expected) {
  for (size_t i=0;i<d.value_count;++i)
    if (d.values[i].name && std::strcmp(d.values[i].name, field)==0)
      return std::fabs(d.values[i].value-expected)<1e-6;
  return false;
}
static bool date_value(const xmq_portable::DecodedPayload &d, const xmq_portable::DecodeDescriptor &descriptor,
  const char *field, int yy, int mm, int dd) {
  for (size_t i=0;i<d.date_count;++i)
    if (std::strcmp(descriptor.tables.date_calculations[i].field,field)==0)
      return d.dates[i].year==yy && d.dates[i].month==mm && d.dates[i].day==dd;
  return false;
}
int main() {
  using namespace xmq_portable;
  Capture captures[16]{};
  CalcValue numeric[32]{};
  CalendarDate dates[4]{};
  DecodedPayload decoded{numeric,32,0,dates,4,0};

  // mkradio4: official fixture 1, meter 02410120
  {
    const uint8_t telegram[] = {0x2f,0x44,0x68,0x50,0x20,0x01,0x41,0x02,0x95,0x62,0xa2,0x06,0x70,0x29,0x01,0x00,0x60,0x17,0x03,0x00,0x04,0x00,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
    wmbus_pure::Header h{};
    assert(wmbus_pure::parse_header(telegram,sizeof(telegram),h)==wmbus_pure::HeaderStatus::Ok);
    assert(wmbus_pure::match_meter_id(h,"02410120")==wmbus_pure::MatchStatus::Match);
    const auto *descriptor=find_decode_descriptor("mkradio4",h.ci);
    assert(descriptor);
    assert(decode_driver_payload(descriptor->tables,h.application,h.application_length,captures,16,decoded)==DriverStatus::Ok);
    assert(near_value(decoded,"total",0.4));
    assert(near_value(decoded,"target",0.1));
  }

  // fhkvdataiii: official fixture 2, meter 11776622
  {
    const uint8_t telegram[] = {0x34,0x44,0x68,0x50,0x22,0x66,0x77,0x11,0x69,0x80,0xa0,0x11,0x9f,0x27,0x02,0x04,0x80,0x04,0x83,0x00,0xc4,0x08,0xf7,0x09,0x14,0x3c,0x00,0x3d,0x34,0x1a,0x2b,0x0b,0x2a,0x07,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0x2d,0x11,0x44,0x57,0x56,0x3d,0x71,0xa1,0x85,0x00,0x00};
    wmbus_pure::Header h{};
    assert(wmbus_pure::parse_header(telegram,sizeof(telegram),h)==wmbus_pure::HeaderStatus::Ok);
    assert(wmbus_pure::match_meter_id(h,"11776622")==wmbus_pure::MatchStatus::Match);
    const auto *descriptor=find_decode_descriptor("fhkvdataiii",0xA0);
    assert(descriptor);
    assert(decode_driver_payload(descriptor->tables,h.application,h.application_length,captures,16,decoded)==DriverStatus::Ok);
    assert(near_value(decoded,"current_hca",131));
    assert(date_value(decoded,*descriptor,"current",2020,2,8));
    assert(near_value(decoded,"previous_hca",1026));
    assert(date_value(decoded,*descriptor,"previous",2019,12,31));
    assert(near_value(decoded,"temp_room_c",22.44));
    assert(near_value(decoded,"temp_radiator_c",25.51));
  }

  // fhkvdataiii: official fixture 3, meter 11111234
  {
    const uint8_t telegram[] = {0x33,0x44,0x68,0x50,0x34,0x12,0x11,0x11,0x94,0x80,0xa2,0x0f,0x9f,0x29,0x2d,0x00,0x50,0x24,0x04,0x00,0x11,0xbd,0x08,0x38,0x09,0x04,0x00,0x00,0x00,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x03,0x14,0x0e};
    wmbus_pure::Header h{};
    assert(wmbus_pure::parse_header(telegram,sizeof(telegram),h)==wmbus_pure::HeaderStatus::Ok);
    assert(wmbus_pure::match_meter_id(h,"11111234")==wmbus_pure::MatchStatus::Match);
    const auto *descriptor=find_decode_descriptor("fhkvdataiii",0xA0);
    assert(descriptor);
    assert(decode_driver_payload(descriptor->tables,h.application,h.application_length,captures,16,decoded)==DriverStatus::Ok);
    assert(near_value(decoded,"current_hca",4));
    assert(date_value(decoded,*descriptor,"current",2021,2,5));
    assert(near_value(decoded,"previous_hca",45));
    assert(date_value(decoded,*descriptor,"previous",2020,12,31));
    assert(near_value(decoded,"temp_room_c",22.37));
    assert(near_value(decoded,"temp_radiator_c",23.6));
  }

  // fhkvdataiii: official fixture 4, meter 41059605
  {
    const uint8_t telegram[] = {0x32,0x44,0x68,0x50,0x05,0x96,0x05,0x41,0x69,0x80,0xa0,0x01,0x9f,0x33,0x48,0x05,0x40,0x0c,0xfe,0x02,0x98,0x08,0xb1,0x08,0x00,0x00,0x00,0x00,0x00,0x06,0x0f,0x4f,0x34,0x67,0xae,0xa9,0xa8,0x8d,0x7b,0x8d,0x50,0x3e,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
    wmbus_pure::Header h{};
    assert(wmbus_pure::parse_header(telegram,sizeof(telegram),h)==wmbus_pure::HeaderStatus::Ok);
    assert(wmbus_pure::match_meter_id(h,"41059605")==wmbus_pure::MatchStatus::Match);
    const auto *descriptor=find_decode_descriptor("fhkvdataiii",0xA0);
    assert(descriptor);
    assert(decode_driver_payload(descriptor->tables,h.application,h.application_length,captures,16,decoded)==DriverStatus::Ok);
    assert(date_value(decoded,*descriptor,"current",2026,6,4));
    assert(near_value(decoded,"current_hca",766));
    assert(date_value(decoded,*descriptor,"previous",2025,12,31));
    assert(near_value(decoded,"previous_hca",1352));
    assert(near_value(decoded,"temp_radiator_c",22.25));
    assert(near_value(decoded,"temp_room_c",22));
  }
  std::puts("PASS 4 official XMQ fixtures on independent portable decoder");
}
