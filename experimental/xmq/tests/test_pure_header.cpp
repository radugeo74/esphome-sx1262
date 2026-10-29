#include "../../components/wmbus_radio/pure_frame_header.h"
#include <cassert>
#include <cstdint>
using namespace wmbus_pure;
int main() {
  // Realistic 51-byte Techem T1-A frame layout (length + link + short header).
  uint8_t f[51]{};
  f[0]=50; f[1]=0x44; f[2]=0x68; f[3]=0x50;
  f[4]=0x67; f[5]=0x30; f[6]=0x27; f[7]=0x24;
  f[8]=0x69; f[9]=0x80; f[10]=0xA0; f[11]=0x11;
  Header h{};
  assert(parse_header(f,sizeof(f),h)==HeaderStatus::Ok);
  assert(h.meter_id==24273067u);
  assert(h.manufacturer==0x5068);
  assert(h.control==0x44 && h.version==0x69 && h.device_type==0x80);
  assert(h.ci==0xA0 && h.application_length==40 && h.application[0]==0x11);
  f[0]=49;
  assert(parse_header(f,sizeof(f),h)==HeaderStatus::InvalidLength);
  f[0]=50; f[4]=0xFA;
  assert(parse_header(f,sizeof(f),h)==HeaderStatus::InvalidId);
  assert(parse_header(nullptr,0,h)==HeaderStatus::TooShort);
}
