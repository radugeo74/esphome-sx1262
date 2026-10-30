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
    0x0A,0x44,0x68,0x50,0x67,0x30,0x27,0x24,0x69,0x80,0xA0
  };
  Header h{};
  assert(parse_header(heat, sizeof(heat), h) == HeaderStatus::Ok);
  assert(h.meter_id == 24273067);
  std::vector<const char *> ids{
    "24273067","24273082","24273600","24273596","24273598","24273595",
    "24273081","22413777","22413819","22413738","22413729","22413715"
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
  assert(!configured_id_is_exact("2427306"));
  assert(!configured_id_is_exact("2427306X"));
  assert(route_to_configured_meter(h, "invalid")); // legacy fallback
  std::puts("Pure configured-ID dispatch PASS");
}
