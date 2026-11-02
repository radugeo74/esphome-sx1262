#include <cassert>
#include <cstdio>
#include <vector>
#include "../../../components/wmbus_radio/pure_meter_dispatch.h"

static unsigned count_routes(const wmbus_pure::Header &h, const std::vector<const char *> &ids) {
  unsigned count = 0;
  for (const auto *id : ids)
    if (wmbus_pure::route_to_configured_meter(h, id)) ++count;
  return count;
}

int main() {
  using namespace wmbus_pure;
  const uint8_t heat[] = {
    0x0A,0x44,0x68,0x50,0x01,0x00,0x00,0x31,0x69,0x80,0xA0
  };
  Header h{};
  assert(parse_header(heat, sizeof(heat), h) == HeaderStatus::Ok);
  assert(h.meter_id == 31000001);
  std::vector<const char *> ids{
    "31000001","31000003","31000004","31000005","31000006","31000007",
    "31000008","31000009","31000010","31000011","31000012","31000013"
  };
  assert(count_routes(h, ids) == 1);
  const char *additional[] = {
    "10000001","10000002","10000003","10000004",
    "10000005","10000006","10000007","10000008"
  };
  ids.insert(ids.end(), additional, additional + 8);
  assert(ids.size() == 20 && count_routes(h, ids) == 1);
  h.meter_id = 99999999;
  assert(count_routes(h, ids) == 0);
  assert(configured_id_is_exact("00000001"));
  assert(!configured_id_is_exact("3100000"));
  assert(!configured_id_is_exact("3100000X"));
  assert(route_to_configured_meter(h, "invalid")); // legacy fallback
  std::puts("Pure configured-ID dispatch PASS");
}
