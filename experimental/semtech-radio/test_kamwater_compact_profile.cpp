#include "components/wmbus_native_sx126x/kamwater_clear_records.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>
static std::vector<uint8_t> hex(const char *s) {
  std::vector<uint8_t> out; int high=-1;
  for(;*s;++s) {
    const int n=(*s>='0'&&*s<='9')?*s-'0':
                (*s>='A'&&*s<='F')?*s-'A'+10:-1;
    assert(n>=0);
    if(high<0) high=n;
    else {out.push_back(static_cast<uint8_t>((high<<4)|n));high=-1;}
  }
  assert(high<0);return out;
}
int main() {
  using namespace xmq_portable;
  // First compact profile is copied from upstream kamwater.xmq, not guessed.
  auto profile=hex("02FF2004134413615B6167");
  // Official compact NOKEY fixture after '_' (already-clear records).
  auto application=hex("710008190000081900007F13");
  uint8_t expanded[64]{};
  size_t used=100;
  assert(compact::expand(profile.data(),profile.size(),
                         application.data(),application.size(),
                         expanded,sizeof(expanded),used)==compact::Status::Ok);
  auto reference=hex("02FF207100041308190000441308190000615B7F616713");
  assert(used==reference.size());
  for(size_t i=0;i<used;++i) assert(expanded[i]==reference[i]);
  kamwater::Reading reading{};
  assert(kamwater::decode_known_clear_compact_profile_0(
      application.data(),application.size(),reading)==kamwater::Status::Ok);
  assert(std::fabs(reading.total_m3-6.408)<1e-8);
  assert(reading.has_target && std::fabs(reading.target_m3-6.408)<1e-8);
  assert(reading.has_status && reading.status_bits==0x71);
  // Second upstream pair: MULTICAL21 (full fixture #3 vs compact #4).
  // The compact RF fixture has proprietary leading F4307912C40DFF.
  // Only the remaining 10 clear data bytes are used here; this test
  // does NOT claim that the envelope authenticates a format signature.
  auto second_profile=hex("02FF20041392013BA1015B8101E7FF0F");
  auto second_payload=hex("00002F4E00003D010203");
  auto second_full=hex(
      "02FF20000004132F4E000092013B3D01A1015B028101E7FF0F03");
  assert(compact::expand(second_profile.data(),second_profile.size(),
                         second_payload.data(),second_payload.size(),
                         expanded,sizeof(expanded),used)==compact::Status::Ok);
  assert(used==second_full.size());
  for(size_t i=0;i<used;++i) assert(expanded[i]==second_full[i]);
  assert(kamwater::decode_known_clear_compact_profile_1(
      second_payload.data(),second_payload.size(),reading)==kamwater::Status::Ok);
  assert(std::fabs(reading.total_m3-20.015)<1e-8);
  assert(!reading.has_target);
  assert(reading.has_status && reading.status_bits==0);
  auto second_truncated=second_payload;
  second_truncated.pop_back();
  assert(kamwater::decode_known_clear_compact_profile_1(
      second_truncated.data(),second_truncated.size(),reading)!=kamwater::Status::Ok);
  assert(reading.total_m3==0 && !reading.has_target);
  auto second_extra=second_payload;
  second_extra.push_back(0);
  assert(kamwater::decode_known_clear_compact_profile_1(
      second_extra.data(),second_extra.size(),reading)!=kamwater::Status::Ok);
  assert(reading.total_m3==0);
  // Truncated and overlong applications may never publish old readings.
  auto truncated=application;
  truncated.pop_back();
  assert(kamwater::decode_known_clear_compact_profile_0(
      truncated.data(),truncated.size(),reading)!=kamwater::Status::Ok);
  assert(reading.total_m3==0 && !reading.has_target);
  auto overlong=application; overlong.push_back(0x00);
  assert(kamwater::decode_known_clear_compact_profile_0(
      overlong.data(),overlong.size(),reading)!=kamwater::Status::Ok);
  assert(reading.total_m3==0);
  // Unsupported variable-length profile, truncated VIFE, and output
  // capacity overflow are all rejected before any sensor publication.
  auto bad=hex("0D13");
  assert(compact::expand(bad.data(),bad.size(),application.data(),
                         application.size(),expanded,sizeof(expanded),used)
         ==compact::Status::Unsupported);
  bad=hex("02FF");
  assert(compact::expand(bad.data(),bad.size(),application.data(),
                         application.size(),expanded,sizeof(expanded),used)
         ==compact::Status::Truncated);
  // An incomplete DIF extension is also truncated, not unsupported.
  bad=hex("8201");
  assert(compact::expand(bad.data(),bad.size(),application.data(),
                         application.size(),expanded,sizeof(expanded),used)
         ==compact::Status::Truncated);
  bad=hex("82");
  assert(compact::expand(bad.data(),bad.size(),application.data(),
                         application.size(),expanded,sizeof(expanded),used)
         ==compact::Status::Truncated);
  assert(compact::expand(profile.data(),profile.size(),
                         application.data(),application.size(),
                         expanded,3,used)==compact::Status::Capacity);
  assert(used==0);
  return 0;
}
