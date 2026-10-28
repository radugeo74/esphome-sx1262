#include "generated_ixml.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
using namespace xmq_portable;

static std::vector<uint8_t> unhex(const char *value) {
  std::vector<uint8_t> bytes;
  for (size_t i = 0; value[i]; i += 2) {
    const int hi = hex_digit(value[i]);
    const int lo = hex_digit(value[i+1]);
    assert(hi >= 0 && lo >= 0);
    bytes.push_back(static_cast<uint8_t>((hi << 4) | lo));
  }
  return bytes;
}
static void check(const Rule *rules, size_t count, const char *hex,
                  const char *field, uint8_t lo, uint8_t hi) {
  const auto input = unhex(hex);
  Capture captures[12]{};
  Result result{captures, 12, 0, 0};
  assert(execute(rules, count, "decode", input.data(), input.size(), result) == ParseStatus::Ok);
  assert(result.consumed == input.size());
  bool found = false;
  for (size_t i = 0; i < result.count; ++i) {
    if (std::strcmp(captures[i].name, field) == 0) {
      assert(captures[i].length == 2);
      assert(captures[i].data[0] == lo && captures[i].data[1] == hi);
      found = true;
    }
  }
  assert(found);
}
int main() {
  check(generated_ixml::mkradio4::rules, generated_ixml::mkradio4::count,
        "06702901006017030004000300000000000000000000000000000000000000000000",
        "DV_curr", 0x03, 0x00);
  check(generated_ixml::fhkvdataiii::rules, generated_ixml::fhkvdataiii::count,
        "0F9F292D005024040011BD08380904000000070000000000000000000000000001000000000003140E",
        "DV_prev_hca", 0x2D, 0x00);
  check(generated_ixml::fhkvdataiii::rules, generated_ixml::fhkvdataiii::count,
        "019F334805400CFE029808B1080000000000060F4F3467AEA9A88D7B8D503E000000000000000000",
        "DV_curr_hca", 0xFE, 0x02);
}
