#pragma once
// Portable, allocation-free iPERL CI=7A plaintext decoder.
// Public upstream fixtures; NOT an AES decryptor or an unchecked CSV reader.
#include <cstddef>
#include <cstdint>
#include "pure_frame_header.h"

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
  bool total = false;
  bool flow = false;
  size_t i = 15;
  while (i < size) {
    const uint8_t dif = frame[i++];
    if (dif == 0x2F) continue;
    if ((dif & 0x80u) != 0) return Status::Unsupported; // DIFE not yet supported
    const uint8_t format = dif & 0x0Fu;
    if (format == 0x0F) return Status::Unsupported;
    if (i >= size) return Status::InvalidFrame;
    const uint8_t vif = frame[i++];
    if ((vif & 0x80u) != 0) return Status::Unsupported; // VIFE extension
    size_t count = 0;
    switch (format) {
      case 0x00: count = 0; break;
      case 0x01: count = 1; break;
      case 0x02: count = 2; break;
      case 0x03: count = 3; break;
      case 0x04: count = 4; break;
      case 0x06: count = 6; break;
      case 0x07: count = 8; break;
      default: return Status::Unsupported;
    }
    if (count > size - i) return Status::InvalidFrame;
    if (vif == 0x13 && format == 4) {
      if (total) return Status::Unsupported;
      out.total_m3 = static_cast<double>(u32le(frame + i)) / 1000.0;
      total = true;
    } else if (vif == 0x3B && format == 2) {
      if (flow) return Status::Unsupported;
      out.max_flow_m3h = static_cast<double>(frame[i] | (uint16_t(frame[i+1]) << 8)) / 1000.0;
      flow = true;
    }
    i += count;
  }
  if (!total || !flow) {
    out = {};
    return Status::MissingFields;
  }
  return Status::Ok;
}
} } // namespace xmq_portable::iperl
