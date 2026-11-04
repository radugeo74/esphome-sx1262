#include "components/wmbus_native_sx126x/standard_dif_vif.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <vector>
#include <utility>
#include <limits>
using namespace xmq_portable::standard;
static Record single(std::initializer_list<uint8_t> bytes) {
  Record rec[4]{};
  size_t count=99;
  const auto status=records(bytes.begin(),bytes.size(),rec,4,count);
  assert(status==Status::Ok && count==1);
  return rec[0];
}
static void near(double actual,double wanted,double tolerance=1e-8) {
  assert(std::fabs(actual-wanted)<tolerance);
}
int main() {
  // Compare canonical conversions against the original wmbus.cc
  // vifScale() table: raw / divisor, never assume an unknown unit.
  for (uint8_t vif=0x10; vif<=0x17; ++vif) {
    double actual=0;
    assert(to_canonical(vif,12345,actual));
    near(actual,12345/std::pow(10.0,0x16 - vif),1e-6);
    assert(canonical_unit(vif)==CanonicalUnit::CubicMeters);
  }
  for (uint8_t vif=0x38; vif<=0x3F; ++vif) {
    double actual=0;
    assert(to_canonical(vif,-345,actual));
    near(actual,-345/std::pow(10.0,0x3E - vif),1e-8);
    assert(canonical_unit(vif)==CanonicalUnit::CubicMetersPerHour);
  }
  // Explicit numeric references from upstream extended flow table.
  for (const auto &entry : std::initializer_list<std::pair<uint8_t,double>>{
      {0x40,600000000.0},{0x47,60.0},{0x48,3600000000000.0},
      {0x4E,3600000.0},{0x4F,360000.0}}) {
    double actual=0;
    assert(to_canonical(entry.first,entry.second,actual));
    near(actual,1.0);
  }
  for (uint8_t vif : {uint8_t{0x58},uint8_t{0x5B},
                       uint8_t{0x64},uint8_t{0x67}}) {
    double actual=0;
    assert(to_canonical(vif,-1000,actual));
    near(actual,vif==0x58 || vif==0x64 ? -1.0 : -1000.0);
    assert(canonical_unit(vif)==CanonicalUnit::Celsius);
  }
  for (const auto &entry : std::initializer_list<std::pair<uint8_t,double>>{
      {0x20,3600.0},{0x21,60.0},{0x22,1.0},{0x23,1.0/24.0},
      {0x68,1000.0},{0x6B,1.0}}) {
    double actual=0;
    assert(to_canonical(entry.first,entry.second,actual));
    near(actual,1.0);
  }
  double unknown=77;
  assert(!to_canonical(0x6C,123,unknown) && unknown==0);
  assert(!to_canonical(0x7D,123,unknown) && unknown==0);
  assert(!to_canonical(0x16,std::numeric_limits<double>::infinity(),unknown));
  // Original public Hydrus records: 0xFD's first VIFE must be consumed.
  auto battery=single({0x02,0xFD,0x74,0x87,0x13});
  assert(battery.vif==0x7D && battery.vif_extension==0x74);
  near(battery.value,4999);
  // Chained VIFE bytes do not become phantom DIF records.
  auto chained=single({0x02,0xFD,0xF4,0x02,0x87,0x13});
  assert(chained.vif_extension==0x74);
  near(chained.value,4999);
  assert(chained.vif_extension_count==2);
  assert(battery.vif_extension_count==1);
  // An explicit VIFE=0 is not interchangeable with an absent extension.
  auto zero_vife=single({0x02,0x93,0x00,0x34,0x12});
  assert(zero_vife.vif==0x13 && zero_vife.vif_extension==0 &&
         zero_vife.vif_extension_count==1);
  auto no_vife=single({0x02,0x13,0x34,0x12});
  assert(no_vife.vif_extension_count==0);

  // Original dvparser: the first VIFE following FD/FB/EF/FF is the
  // extended base, subsequent VIFEs are combinable modifiers.
  assert(battery.full_vif==0x7D74 && battery.modifier_count==0);
  assert(chained.full_vif==0x7D74 && chained.modifier_count==1 &&
         chained.modifiers[0]==2);
  auto secondary=single({0x02,0xFB,0x1A,0x34,0x12});
  assert(secondary.full_vif==0x7B1A && secondary.modifier_count==0);
  auto special=single({0x02,0xEF,0x02,0x34,0x12});
  assert(special.full_vif==0x6F02 && special.modifier_count==0);
  auto manufacturer=single({0x02,0xFF,0x02,0x34,0x12});
  assert(manufacturer.full_vif==0x7F02 && manufacturer.modifier_count==0);
  auto combinable=single({0x02,0x93,0xFC,0x03,0x34,0x12});
  assert(combinable.full_vif==0x13 && combinable.modifier_count==1 &&
         combinable.modifiers[0]==0x7C03);
  const uint8_t unfinished_modifier[]={0x02,0x93,0x7C,0x34,0x12};
  Record bad_mod[1]{}; size_t bad_count=9;
  assert(records(unfinished_modifier,sizeof(unfinished_modifier),
                 bad_mod,1,bad_count)==Status::Unsupported);
  assert(bad_count==0);
  // A DIFE supplies four storage bits, two tariff bits, one subunit bit.
  auto dife=single({0xC4,0x51,0x13,0x15,0x00,0x00,0x00});
  assert(dife.storage==3 && dife.tariff==1 && dife.subunit==1);
  near(dife.value,21);
  // Two DIFE octets use disjoint storage/tariff/subunit positions.
  auto multi=single({0x84,0xF1,0x52,0x13,0x01,0x00,0x00,0x00});
  assert(multi.storage==1+2*16+2*16*16 || multi.storage==static_cast<uint32_t>((1<<1)|(2<<5)));
  assert(multi.tariff==3+1*4 && multi.subunit==3);
  // 48-bit and 64-bit signed little-endian values.
  near(single({0x06,0x13,0xFE,0xFF,0xFF,0xFF,0xFF,0xFF}).value,-2);
  near(single({0x07,0x13,0xFD,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}).value,-3);
  // 32-bit IEEE-754 real: 3.5 and -2.25.
  near(single({0x05,0x13,0x00,0x00,0x60,0x40}).value,3.5);
  near(single({0x05,0x13,0x00,0x00,0x10,0xC0}).value,-2.25);
  // Negative BCD sign nibble and VIFE length checking.
  near(single({0x0C,0x13,0x34,0x12,0x00,0xF0}).value,-1234);
  Record records_out[2]{};
  size_t count=2;
  const uint8_t truncated[]={0x02,0xFD,0xF4};
  assert(records(truncated,sizeof(truncated),records_out,2,count)==Status::Invalid);
  assert(count==0);
  const uint8_t bad_float[]={0x05,0x13,0x00,0x00,0xC0,0x7F};
  assert(records(bad_float,sizeof(bad_float),records_out,2,count)==Status::Unsupported);
  // One valid record followed by a truncated record must never yield
  // partial values, even though the first record itself was valid.
  const uint8_t partial[]={0x02,0x13,0x39,0x30,0x02,0xFD};
  count=2;
  assert(records(partial,sizeof(partial),records_out,2,count)==Status::Invalid);
  assert(count==0);
  // Every recognized LVAR family must skip exactly its payload length,
  // leaving the next numeric DIF/VIF aligned. None is published as numeric.
  for (uint8_t lvar : {uint8_t{0xC2},uint8_t{0xD2},
                       uint8_t{0xE2},uint8_t{0xF0},uint8_t{0xF5},
                       uint8_t{0xF6}}) {
    const size_t bytes = lvar == 0xF0 ? 16 :
                         lvar == 0xF5 ? 48 :
                         lvar == 0xF6 ? 64 : 2;
    std::vector<uint8_t> frame{0x0D,0x13,lvar};
    frame.insert(frame.end(),bytes,0);
    frame.insert(frame.end(),{0x02,0x13,0x34,0x12});
    Record entries[3]{};
    size_t found=0;
    assert(records(frame.data(),frame.size(),entries,3,found)==Status::Ok);
    assert(found==2 && !entries[0].numeric && entries[1].numeric);
    near(entries[1].value,4660);
    frame.pop_back();
    found=99;
    assert(records(frame.data(),frame.size(),entries,3,found)==Status::Invalid);
    assert(found==0);
  }
  // Reserved LVAR values are rejected rather than guessed.
  const uint8_t reserved_lvar[]={0x0D,0x13,0xFA,0x02,0x13,0x34,0x12};
  count=2;
  assert(records(reserved_lvar,sizeof(reserved_lvar),records_out,2,count)==Status::Unsupported);
  assert(count==0);
  // LVAR stays opaque and cannot be mistaken for a numeric meter value.
  auto text=single({0x0D,0x7C,0x03,0x41,0x42,0x43,0x03,0x31,0x32,0x33});
  assert(!text.numeric);
  return 0;
}
