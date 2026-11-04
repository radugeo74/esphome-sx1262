#include "components/wmbus_native_sx126x/native_identity_probe.h"
#include <cassert>
#include <cstdint>
#include <vector>
using namespace wmbus_pure::identity;
static std::vector<uint8_t> frame(uint8_t ci) {
  // Synthetic DLL: TCH converter ID 00464936, with distinct TPL meter
  // ID 66953825 and manufacturer HYD (not user-specific data).
  std::vector<uint8_t> v{
    0x00,0x44,0x68,0x50,0x36,0x49,0x46,0x00,0x95,0x37,ci
  };
  if (ci==0x72) {
    const uint8_t tpl[]={0x25,0x38,0x95,0x66,0x24,0x23,
                         0xFE,0x06,0x4E,0x00,0x30,0x05};
    v.insert(v.end(),tpl,tpl+sizeof(tpl));
  } else if (ci==0x7A) {
    v.insert(v.end(),{0x4E,0x00,0x00,0x00});
  } else {
    v.push_back(0x00);
  }
  v[0]=static_cast<uint8_t>(v.size()-1);
  return v;
}
int main() {
  auto raw=frame(0x72);
  auto parsed=inspect(raw.data(),raw.size());
  assert(parsed.status==Status::Ok && parsed.has_tpl);
  assert(parsed.application_offset==23);
  assert(parsed.dll.decimal_bcd && parsed.tpl.decimal_bcd);
  assert(match(parsed,"00464936")==Source::Dll);
  assert(match(parsed,"66953825")==Source::Tpl);
  assert(match(parsed,"12345678")==Source::None);
  assert(match(parsed,"6695382")==Source::None);
  assert(match(parsed,"6695382X")==Source::None);
  // TPL ID must not be accidentally interpreted as the DLL converter ID.
  assert(parsed.dll.manufacturer==0x5068);
  assert(parsed.tpl.manufacturer==0x2324);
  auto same=raw;
  for (int i=0;i<4;++i) same[11+i]=same[4+i];
  parsed=inspect(same.data(),same.size());
  assert(match(parsed,"00464936")==Source::Both);
  auto short_header=frame(0x7A);
  parsed=inspect(short_header.data(),short_header.size());
  assert(parsed.status==Status::Ok && !parsed.has_tpl &&
         match(parsed,"00464936")==Source::Dll);
  auto vendor=frame(0xA2);
  parsed=inspect(vendor.data(),vendor.size());
  assert(parsed.status==Status::Ok && !parsed.has_tpl);
  // ELL is intentionally unclassified until its header is parsed safely.
  auto ell=frame(0x8C);
  assert(inspect(ell.data(),ell.size()).status==Status::UnsupportedCi);
  // Non-BCD IDs are valid diagnostic addresses as in original wmbusmeters,
  // but never silently converted to numeric DLL IDs.
  raw[7]=0x8B;
  parsed=inspect(raw.data(),raw.size());
  assert(parsed.status==Status::Ok && !parsed.dll.decimal_bcd);
  assert(match(parsed,"8B464936")==Source::Dll);
  assert(match(parsed,"66953825")==Source::Tpl);
  assert(match(parsed,"88464936")==Source::None);
  // Bad/truncated manufacturer, invalid lengths and headers are rejected.
  raw[0]++;
  assert(inspect(raw.data(),raw.size()).status==Status::InvalidLength);
  raw[0]--;
  raw[2]=0;
  raw[3]=0;
  assert(inspect(raw.data(),raw.size()).status==Status::InvalidManufacturer);
  auto truncated=frame(0x72);
  truncated.resize(22); truncated[0]=21;
  assert(inspect(truncated.data(),truncated.size()).status==Status::TooShort);
  auto invalid_tpl=frame(0x72);
  invalid_tpl[15]=0;invalid_tpl[16]=0;
  assert(inspect(invalid_tpl.data(),invalid_tpl.size()).status==Status::InvalidTplManufacturer);
  assert(inspect(nullptr,0).status==Status::TooShort);
  return 0;
}
