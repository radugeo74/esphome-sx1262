#pragma once
// Portable, allocation-free iPERL CI=7A plaintext decoder.
// Public upstream fixtures; NOT an AES decryptor or an unchecked CSV reader.
#include <cstddef>
#include <cstdint>
#include "pure_frame_header.h"
#include "standard_dif_vif.h"

namespace xmq_portable { namespace iperl {
enum class Status : uint8_t { Ok, InvalidFrame, Unsupported, Encrypted, MissingFields };
struct Reading {
  double total_m3{0};
  double max_flow_m3h{0};
  uint8_t tpl_status{0};
  bool power_low{false};
};
inline uint32_t u32le(const uint8_t *p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
         (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
inline Status decode(const uint8_t *frame, size_t size, Reading &out) {
  out = {};
  wmbus_pure::Header h{};
  if (wmbus_pure::parse_header(frame, size, h) != wmbus_pure::HeaderStatus::Ok)
    return Status::InvalidFrame;
  // Sensus iPERL: manufacturer SEN, version 0x68/0x7c and cold/warm water.
  if (h.manufacturer != 0x4CAE || (h.version != 0x68 && h.version != 0x7C) ||
      (h.device_type != 0x06 && h.device_type != 0x07) || h.ci != 0x7A)
    return Status::Unsupported;
  if (size < 15) return Status::InvalidFrame;
  const uint16_t config = uint16_t(frame[13]) | (uint16_t(frame[14]) << 8);
  // Security modes other than zero require a validated decrypt stage.
  if (((config >> 8) & 0x0F) != 0) return Status::Encrypted;
  out.tpl_status = frame[12];
  out.power_low = (frame[12] & 0x04u) != 0; // upstream TPL 0x06 POWER_LOW fixture
  // Shared DIF/VIF records interpret upstream iPERL fields; no hard-coded
  // byte offsets, but strict quantity, storage, and function matching.
  standard::Record rec[24]{};
  size_t count = 0;
  if (standard::records(frame + 15, size - 15, rec, 24, count) != standard::Status::Ok)
    return Status::Unsupported;
  bool total = false;
  bool flow = false;
  for (size_t i = 0; i < count; ++i) {
    const auto &r = rec[i];
    if (!r.numeric || r.function != 0 || r.storage != 0 || r.tariff != 0)
      continue;
    if (r.vif >= 0x10 && r.vif <= 0x17) {
      if (total) return Status::Unsupported;
      out.total_m3 = r.value * standard::scale10(static_cast<int>(r.vif) - 0x16);
      total = true;
    } else if (r.vif >= 0x38 && r.vif <= 0x3F) {
      if (flow) return Status::Unsupported;
      out.max_flow_m3h = r.value * standard::scale10(static_cast<int>(r.vif) - 0x3E);
      flow = true;
    }
  }
  if (!total || !flow) {
    out = {};
    return Status::MissingFields;
  }
  return Status::Ok;
}
} } // namespace xmq_portable::iperl
