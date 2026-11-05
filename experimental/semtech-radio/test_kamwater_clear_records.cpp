#include "components/wmbus_native_sx126x/kamwater_clear_records.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>
static std::vector<uint8_t> hex(const char *s) {
  std::vector<uint8_t> out;
  int hi=-1;
  while(*s) {
    const char c=*s++;
    const int n=(c>='0'&&c<='9')?c-'0':
                (c>='A'&&c<='F')?c-'A'+10:-1;
    assert(n>=0);
    if(hi<0) hi=n;
    else {out.push_back(static_cast<uint8_t>((hi<<4)|n));hi=-1;}
  }
  assert(hi<0); return out;
}
int main() {
  using namespace xmq_portable::kamwater;
  Reading reading{};
  // Official kamwater.xmq test #1: already-clear application AFTER '_' .
  // ELL 8D20 is not proven clear RF, so only test the supplied application.
  auto multical=hex("02FF207100041308190000441308190000615B7F616713");
  assert(decode_clear_records(multical.data(),multical.size(),reading)==Status::Ok);
  assert(std::fabs(reading.total_m3-6.408)<1e-9);
  assert(reading.has_target && std::fabs(reading.target_m3-6.408)<1e-9);
  assert(reading.has_status && reading.status_bits==0x71);
  // Official flowIQ fixture #5: clear application DIF/VIF, with status,
  // target storage 1, historical target date and total storage 0.
  auto flowiq=hex("04FF23000000000413AEAC0000441364A80000426C812A023B000092013BEF01A2013B000006FF1B067000097000A1015B0C91015B14A1016713");
  assert(decode_clear_records(flowiq.data(),flowiq.size(),reading)==Status::Ok);
  assert(std::fabs(reading.total_m3-44.206)<1e-9);
  assert(reading.has_target && std::fabs(reading.target_m3-43.108)<1e-9);
  assert(reading.has_status && reading.status_bits==0);
  // Official flowIQ fixture #6: target 0 and total 3.871m3.
  auto cold=hex("04FF2300080000441300000000523B000006FF1B08900008F0FF426CC12B61670A51671B023B000004131F0F00008101E7FF0F13");
  assert(decode_clear_records(cold.data(),cold.size(),reading)==Status::Ok);
  assert(std::fabs(reading.total_m3-3.871)<1e-9);
  assert(reading.has_target && reading.target_m3==0);
  // No stale readings on incomplete, non-volume or ambiguous input.
  auto truncated=flowiq;
  truncated.pop_back();
  assert(decode_clear_records(truncated.data(),truncated.size(),reading)!=Status::Ok);
  assert(reading.total_m3==0 && !reading.has_target);
  auto no_total=hex("02FF207100441308190000");
  assert(decode_clear_records(no_total.data(),no_total.size(),reading)==Status::MissingTotal);
  assert(reading.total_m3==0);
  auto ambiguous=hex("041308190000041309190000");
  assert(decode_clear_records(ambiguous.data(),ambiguous.size(),reading)==Status::Ambiguous);
  return 0;
}
