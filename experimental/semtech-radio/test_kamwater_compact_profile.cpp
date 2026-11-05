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
  // Third official flowIQ pair: full snapshot #6 vs compact snapshot #7.
  // Their instantaneous flow legitimately differs (0 vs 0.035 m3/h).
  // Compare the compact expansion against the reference with that field
  // substituted, rather than falsely claiming identical snapshots.
  auto flowiq_profile=hex(
      "04FF234413523B06FF1B426C61675167023B04138101E7FF0F");
  auto flowiq_compact=hex(
      "0008000000000000000008900008F0FFC12B0A1B23001F0F000013");
  auto flowiq_expanded=hex(
      "04FF2300080000441300000000523B000006FF1B08900008F0FF"
      "426CC12B61670A51671B023B230004131F0F00008101E7FF0F13");
  assert(compact::expand(flowiq_profile.data(),flowiq_profile.size(),
                         flowiq_compact.data(),flowiq_compact.size(),
                         expanded,sizeof(expanded),used)==compact::Status::Ok);
  assert(used==flowiq_expanded.size());
  for(size_t i=0;i<used;++i) assert(expanded[i]==flowiq_expanded[i]);
  assert(kamwater::decode_known_clear_compact_profile_2(
      flowiq_compact.data(),flowiq_compact.size(),reading)==kamwater::Status::Ok);
  assert(std::fabs(reading.total_m3-3.871)<1e-8);
  assert(reading.has_target && reading.target_m3==0);
  assert(reading.has_status && reading.status_bits==0x800);
  auto flowiq_short=flowiq_compact;
  flowiq_short.pop_back();
  assert(kamwater::decode_known_clear_compact_profile_2(
      flowiq_short.data(),flowiq_short.size(),reading)!=kamwater::Status::Ok);
  assert(reading.total_m3==0 && !reading.has_target);
  auto flowiq_extra=flowiq_compact;
  flowiq_extra.push_back(0);
  assert(kamwater::decode_known_clear_compact_profile_2(
      flowiq_extra.data(),flowiq_extra.size(),reading)!=kamwater::Status::Ok);
  assert(reading.total_m3==0);
  // Fourth official XMQ compact profile has NO paired upstream telegram.
  // This is explicitly a synthetic structural contract, NOT RF validation.
  auto fourth_profile=hex(
      "02FF2004134413426C523B615B616751678101E7FF0F06FF1B");
  auto fourth_payload=hex(
      "000039300000E02E0000812A000000001300010203040506");
  auto fourth_expected=hex(
      "02FF2000000413393000004413E02E0000426C812A523B0000"
      "615B006167005167138101E7FF0F0006FF1B010203040506");
  assert(compact::expand(fourth_profile.data(),fourth_profile.size(),
                         fourth_payload.data(),fourth_payload.size(),
                         expanded,sizeof(expanded),used)==compact::Status::Ok);
  assert(used==fourth_expected.size());
  for(size_t i=0;i<used;++i) assert(expanded[i]==fourth_expected[i]);
  assert(kamwater::decode_known_clear_compact_profile_3(
      fourth_payload.data(),fourth_payload.size(),reading)==kamwater::Status::Ok);
  assert(std::fabs(reading.total_m3-12.345)<1e-8);
  assert(reading.has_target && std::fabs(reading.target_m3-12.0)<1e-8);
  assert(reading.has_status && reading.status_bits==0);
  auto fourth_truncated=fourth_payload;
  fourth_truncated.pop_back();
  assert(kamwater::decode_known_clear_compact_profile_3(
      fourth_truncated.data(),fourth_truncated.size(),reading)!=kamwater::Status::Ok);
  assert(reading.total_m3==0 && !reading.has_target);
  auto fourth_extra=fourth_payload;
  fourth_extra.push_back(0);
  assert(kamwater::decode_known_clear_compact_profile_3(
      fourth_extra.data(),fourth_extra.size(),reading)!=kamwater::Status::Ok);
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
