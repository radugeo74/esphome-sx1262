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
  // Fifth upstream XMQ profile has no paired source telegram and is
  // explicitly marked "Unknown if KamHeat or KamWater" in the source.
  // Synthetic ONLY: decoding bytes must NOT resolve meter/driver identity.
  auto fifth_profile=hex("02FF2004134413A1015B8101E7FF0F");
  auto fifth_payload=hex("000039300000E02E00000013");
  auto fifth_expected=hex(
      "02FF2000000413393000004413E02E0000A1015B008101E7FF0F13");
  assert(compact::expand(fifth_profile.data(),fifth_profile.size(),
                         fifth_payload.data(),fifth_payload.size(),
                         expanded,sizeof(expanded),used)==compact::Status::Ok);
  assert(used==fifth_expected.size());
  for(size_t i=0;i<used;++i) assert(expanded[i]==fifth_expected[i]);
  assert(kamwater::decode_known_clear_compact_profile_4(
      fifth_payload.data(),fifth_payload.size(),reading)==kamwater::Status::Ok);
  assert(std::fabs(reading.total_m3-12.345)<1e-8);
  assert(reading.has_target && std::fabs(reading.target_m3-12.0)<1e-8);
  assert(reading.has_status && reading.status_bits==0);
  auto fifth_short=fifth_payload;
  fifth_short.pop_back();
  assert(kamwater::decode_known_clear_compact_profile_4(
      fifth_short.data(),fifth_short.size(),reading)!=kamwater::Status::Ok);
  assert(reading.total_m3==0 && !reading.has_target);
  auto fifth_extra=fifth_payload;
  fifth_extra.push_back(0);
  assert(kamwater::decode_known_clear_compact_profile_4(
      fifth_extra.data(),fifth_extra.size(),reading)!=kamwater::Status::Ok);
  assert(reading.total_m3==0 && !reading.has_target);
  // Sixth official XMQ profile has no compact/full golden pair.
  // Synthetic-only format expansion, never a verified RF signature.
  auto sixth_profile=hex("02FF2004134413615B5167");
  auto sixth_payload=hex("000039300000E02E00001314");
  auto sixth_expected=hex(
      "02FF2000000413393000004413E02E0000615B13516714");
  assert(compact::expand(sixth_profile.data(),sixth_profile.size(),
                         sixth_payload.data(),sixth_payload.size(),
                         expanded,sizeof(expanded),used)==compact::Status::Ok);
  assert(used==sixth_expected.size());
  for(size_t i=0;i<used;++i) assert(expanded[i]==sixth_expected[i]);
  assert(kamwater::decode_known_clear_compact_profile_5(
      sixth_payload.data(),sixth_payload.size(),reading)==kamwater::Status::Ok);
  assert(std::fabs(reading.total_m3-12.345)<1e-8);
  assert(reading.has_target && std::fabs(reading.target_m3-12.0)<1e-8);
  assert(reading.has_status && reading.status_bits==0);
  auto sixth_short=sixth_payload;
  sixth_short.pop_back();
  assert(kamwater::decode_known_clear_compact_profile_5(
      sixth_short.data(),sixth_short.size(),reading)!=kamwater::Status::Ok);
  assert(reading.total_m3==0 && !reading.has_target);
  auto sixth_extra=sixth_payload;
  sixth_extra.push_back(0);
  assert(kamwater::decode_known_clear_compact_profile_5(
      sixth_extra.data(),sixth_extra.size(),reading)!=kamwater::Status::Ok);
  assert(reading.total_m3==0 && !reading.has_target);
  // Seventh original XMQ compact format, no paired upstream fixture.
  // Synthetic-only: explicit profile, status + total + target.
  auto seventh_profile=hex("02FF2004134413");
  auto seventh_payload=hex("000039300000E02E0000");
  auto seventh_expected=hex(
      "02FF2000000413393000004413E02E0000");
  assert(compact::expand(seventh_profile.data(),seventh_profile.size(),
                         seventh_payload.data(),seventh_payload.size(),
                         expanded,sizeof(expanded),used)==compact::Status::Ok);
  assert(used==seventh_expected.size());
  for(size_t i=0;i<used;++i) assert(expanded[i]==seventh_expected[i]);
  assert(kamwater::decode_known_clear_compact_profile_6(
      seventh_payload.data(),seventh_payload.size(),reading)==kamwater::Status::Ok);
  assert(std::fabs(reading.total_m3-12.345)<1e-8);
  assert(reading.has_target && std::fabs(reading.target_m3-12.0)<1e-8);
  assert(reading.has_status && reading.status_bits==0);
  auto seventh_short=seventh_payload;
  seventh_short.pop_back();
  assert(kamwater::decode_known_clear_compact_profile_6(
      seventh_short.data(),seventh_short.size(),reading)!=kamwater::Status::Ok);
  assert(reading.total_m3==0 && !reading.has_target);
  auto seventh_extra=seventh_payload;
  seventh_extra.push_back(0);
  assert(kamwater::decode_known_clear_compact_profile_6(
      seventh_extra.data(),seventh_extra.size(),reading)!=kamwater::Status::Ok);
  assert(reading.total_m3==0 && !reading.has_target);
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
