#include <cassert>
#include <cstring>
#include <cstdio>
#include "../../../components/wmbus_meter/xmq_decode_catalog.h"
int main() {
  using xmq_portable::find_decode_descriptor;
  const auto *heat = find_decode_descriptor("fhkvdataiii", 0xA0);
  const auto *water = find_decode_descriptor("mkradio4", 0xA2);
  assert(heat && water);
  assert(heat->payload_policy == xmq_portable::PayloadPolicy::PreferManufacturer0f);
  assert(water->payload_policy == xmq_portable::PayloadPolicy::Application);
  assert(heat->tables.rules && heat->tables.rule_count > 0);
  assert(water->tables.rules && water->tables.rule_count > 0);
  assert(heat->tables.date_calculation_count > 0);
  assert(water->tables.date_calculation_count == 0);
  assert(find_decode_descriptor("fhkvdataiii", 0xA2) == nullptr);
  assert(find_decode_descriptor("mkradio4", 0xA0) == nullptr);
  assert(find_decode_descriptor("unknown", 0xA0) == nullptr);
  assert(find_decode_descriptor(nullptr, 0xA0) == nullptr);
  const uint8_t frame[]{0xAA, 0x0F, 0x11, 0x22, 0xBB};
  xmq_portable::PayloadSpan payload{};
  assert(xmq_portable::descriptor_payload(*heat, frame, sizeof(frame), 1, 1, 0, payload)
         == xmq_portable::PayloadStatus::Ok);
  assert(payload.data == frame + 1 && payload.length == 3);
  assert(xmq_portable::descriptor_payload(*heat, frame, sizeof(frame), 1, 1, -1, payload)
         == xmq_portable::PayloadStatus::Ok);
  assert(payload.data == frame + 1 && payload.length == 3);
  assert(xmq_portable::descriptor_payload(*heat, frame, sizeof(frame), 1, 1, 2, payload)
         == xmq_portable::PayloadStatus::InvalidBounds);
  assert(xmq_portable::descriptor_payload(*water, frame, sizeof(frame), 1, 1, 2, payload)
         == xmq_portable::PayloadStatus::Ok);
  std::puts("XMQ decode catalog PASS");
}
