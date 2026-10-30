#include <cassert>
#include <cstring>
#include <cstdio>
#include "../../../components/wmbus_meter/xmq_decode_catalog.h"
int main() {
  using xmq_portable::find_decode_descriptor;
  const auto *heat = find_decode_descriptor("fhkvdataiii", 0xA0);
  const auto *water = find_decode_descriptor("mkradio4", 0xA2);
  assert(heat && water);
  assert(heat->tables.rules && heat->tables.rule_count > 0);
  assert(water->tables.rules && water->tables.rule_count > 0);
  assert(heat->tables.date_calculation_count > 0);
  assert(water->tables.date_calculation_count == 0);
  assert(find_decode_descriptor("fhkvdataiii", 0xA2) == nullptr);
  assert(find_decode_descriptor("mkradio4", 0xA0) == nullptr);
  assert(find_decode_descriptor("unknown", 0xA0) == nullptr);
  assert(find_decode_descriptor(nullptr, 0xA0) == nullptr);
  std::puts("XMQ decode catalog PASS");
}
