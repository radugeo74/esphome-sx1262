#include "../../../components/wmbus_radio/pure_frame_layout.h"
#include <cassert>
using namespace wmbus_pure;
int main() {
  RawLayout out{};
  const uint8_t c1a[]{0x54,0xCD,0x30};
  assert(inspect_raw_layout(c1a,sizeof(c1a),RawMode::C1,out)==LayoutStatus::Ok);
  assert(out.format==RawFormat::A && out.l_field==48);
  assert(out.expected_decoded_bytes==59);
  const uint8_t c1b[]{0x54,0x3D,0x30};
  assert(inspect_raw_layout(c1b,sizeof(c1b),RawMode::C1,out)==LayoutStatus::Ok);
  assert(out.format==RawFormat::B && out.expected_decoded_bytes==51);
  const uint8_t invalid[]{0x54,0xAA,0x30};
  assert(inspect_raw_layout(invalid,sizeof(invalid),RawMode::C1,out)==LayoutStatus::UnsupportedFormat);
  assert(inspect_raw_layout(nullptr,0,RawMode::C1,out)==LayoutStatus::TooShort);
  assert(inspect_raw_layout(c1a,sizeof(c1a),RawMode::T1,out)==LayoutStatus::UnsupportedFormat);
}
