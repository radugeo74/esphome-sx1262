#include "components/wmbus_native_sx126x/minomess_clear_prefix.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>
static std::vector<uint8_t> unhex(const char *p) {
  std::vector<uint8_t> out;
  while (*p) {
    const auto digit=[](char c)->uint8_t {
      if (c>='0'&&c<='9') return c-'0';
      if (c>='A'&&c<='F') return c-'A'+10;
      assert(false); return 0;
    };
    const auto a=digit(*p++); assert(*p);
    const auto b=digit(*p++);
    out.push_back(static_cast<uint8_t>((a<<4)|b));
  }
  return out;
}
int main() {
  using namespace xmq_portable::minomess;
  Reading reading{};
  // These are the first standard/rewritten records from unmodified public
  // minomess.xmq post-decryption fixtures. No real user RF data.
  auto absent=unhex("0C1359000000026CBE2B82046CA12B8C0413FFFFFFFF");
  assert(decode_clear_prefix(absent.data(),absent.size(),reading)==Status::Ok);
  assert(std::fabs(reading.total_m3-0.059)<1e-8);
  assert(!reading.has_target);
  assert(reading.has_meter_date && reading.has_target_date);
  assert(reading.meter_date.year==2021 && reading.meter_date.month==11 &&
         reading.meter_date.day==30);
  assert(reading.target_date.year==2021 && reading.target_date.month==11 &&
         reading.target_date.day==1);

  auto cold=unhex("0C1355000000026CEC2182046CE1218C041300000080");
  assert(decode_clear_prefix(cold.data(),cold.size(),reading)==Status::Ok);
  assert(std::fabs(reading.total_m3-0.055)<1e-8);
  assert(!reading.has_target); // -2147483.648 is the upstream null sentinel.

  auto warm=unhex("0C1357000000026CEC2182046CE1218C041300000080");
  assert(decode_clear_prefix(warm.data(),warm.size(),reading)==Status::Ok);
  assert(std::fabs(reading.total_m3-0.057)<1e-8 && !reading.has_target);

  auto populated=unhex("0C1354031400026C4F3A82046C413A8C0413F51C0200");
  assert(decode_clear_prefix(populated.data(),populated.size(),reading)==Status::Ok);
  assert(std::fabs(reading.total_m3-140.354)<1e-8);
  assert(reading.has_target && std::fabs(reading.target_m3-138.485)<1e-8);
  assert(reading.meter_date.year==2026 && reading.meter_date.month==10 &&
         reading.meter_date.day==15);
  assert(reading.target_date.year==2026 && reading.target_date.month==10 &&
         reading.target_date.day==1);

  // Malformed DIF, invalid BCD and truncated value never keep old readings.
  populated[0]=0x0D;
  assert(decode_clear_prefix(populated.data(),populated.size(),reading)
         ==Status::Unsupported);
  assert(reading.total_m3==0 && !reading.has_target);
  populated[0]=0x0C; populated[2]=0xFA;
  assert(decode_clear_prefix(populated.data(),populated.size(),reading)
         ==Status::InvalidBcd);
  assert(reading.total_m3==0);
  populated[2]=0x54;
  assert(decode_clear_prefix(populated.data(),21,reading)==Status::Truncated);
  auto invalid_date=populated; invalid_date[8]=0;
  assert(decode_clear_prefix(invalid_date.data(),invalid_date.size(),reading)
         ==Status::InvalidDate);
  return 0;
}
