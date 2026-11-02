#include "../../../components/wmbus_radio/pure_frame_header.h"
#include "../../../components/wmbus_radio/pure_meter_match.h"
#include <cassert>
#include <cstdint>
using namespace wmbus_pure;
int main() {
  // Realistic 51-byte Techem T1-A frame layout (length + link + short header).
  uint8_t f[51]{};
  f[0]=50; f[1]=0x44; f[2]=0x68; f[3]=0x50;
  f[4]=0x01; f[5]=0x00; f[6]=0x00; f[7]=0x31;
  f[8]=0x69; f[9]=0x80; f[10]=0xA0; f[11]=0x11;
  Header h{};
  assert(parse_header(f,sizeof(f),h)==HeaderStatus::Ok);
  assert(h.meter_id==31000001u);
  assert(h.manufacturer==0x5068);
  assert(h.control==0x44 && h.version==0x69 && h.device_type==0x80);
  assert(h.ci==0xA0 && h.application_length==40 && h.application[0]==0x11);
  assert(match_meter_id(h,"31000001")==MatchStatus::Match);
  assert(match_meter_id(h,"31000002")==MatchStatus::DifferentId);
  assert(match_meter_id(h,"00000000")==MatchStatus::DifferentId);
  assert(match_meter_id(h,"31A00001")==MatchStatus::InvalidConfiguredId);
  assert(match_meter_id(h,"3100000")==MatchStatus::InvalidConfiguredId);
  f[0]=49;
  assert(parse_header(f,sizeof(f),h)==HeaderStatus::InvalidLength);
  f[0]=50; f[4]=0xFA;
  assert(parse_header(f,sizeof(f),h)==HeaderStatus::InvalidId);
  assert(parse_header(nullptr,0,h)==HeaderStatus::TooShort);
  f[4]=0x01; f[2]=0x00; f[3]=0x00;
  assert(parse_header(f,sizeof(f),h)==HeaderStatus::InvalidManufacturer);
  f[2]=0x68; f[3]=0x50;
  assert(parse_header(f,sizeof(f),h)==HeaderStatus::Ok);
  // Captured valid T1-A headers from known Techem water and heat meters.
  // Inputs here are CRC-stripped frames, as delivered by the radio layer.
  const uint8_t water[48] = {
    0x2f,0x44,0x68,0x50,0x13,0x00,0x00,0x31,0x95,0x62,0xa2,
    0x06,0x3e,0x35,0x02,0x00,0x10,0x16
  };
  assert(parse_header(water,sizeof(water),h)==HeaderStatus::Ok);
  assert(h.meter_id==31000013u && h.manufacturer==0x5068u);
  assert(h.version==0x95 && h.device_type==0x62 && h.ci==0xa2);
  assert(h.application==water+11 && h.application_length==37);
  assert(match_meter_id(h,"31000013")==MatchStatus::Match);
  assert(match_meter_id(h,"31000014")==MatchStatus::DifferentId);
  // Valid header does not make the payload trustworthy: CRC/decryption
  // must be checked by an independent preceding stage.
  uint8_t corrupt[sizeof(water)]{};
  for (size_t i=0;i<sizeof(water);++i) corrupt[i]=water[i];
  corrupt[4]=0xfa;
  assert(parse_header(corrupt,sizeof(corrupt),h)==HeaderStatus::InvalidId);
  corrupt[4]=0x13; corrupt[0]=0x2e;
  assert(parse_header(corrupt,sizeof(corrupt),h)==HeaderStatus::InvalidLength);

}
