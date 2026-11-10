#pragma once
// Portable, allocation-free iPERL CI=7A plaintext decoder.
// Public upstream fixtures; NOT an AES decryptor or an unchecked CSV reader.
#include <cstddef>
#include <cstdint>
#include "pure_frame_header.h"
#include "standard_dif_vif.h"
#include "tpl_security_probe.h"

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
// Pure offline parser for application bytes which an upstream XMQ test
// might ALREADY have decrypted. This helper is NOT an RF entrypoint;
// only decode() below can authorize a complete on-air clear packet.
inline Status decode_clear_application(const uint8_t *payload,size_t payload_size,
                                       Reading &out) {
  out = {};
  if(!payload || payload_size==0) return Status::InvalidFrame;
  // Shared DIF/VIF records interpret upstream iPERL fields; no hard-coded
  // byte offsets, but strict quantity, storage, and function matching.
  standard::Record rec[24]{};
  size_t count = 0;
  if (standard::records(payload, payload_size, rec, 24, count) != standard::Status::Ok)
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
// Strict on-air decoder: Mode 0 with N=0 only, never NOKEY-by-comment.
inline Status decode(const uint8_t *frame, size_t size, Reading &out) {
  out = {};
  wmbus_pure::Header h{};
  if (wmbus_pure::parse_header(frame, size, h) != wmbus_pure::HeaderStatus::Ok)
    return Status::InvalidFrame;
  // Exact source MVT triples: SEN/68/06, SEN/68/07, SEN/7C/07.
  // In particular, SEN/7C/06 has no source detection contract.
  const bool model=(h.version==0x68 &&
                    (h.device_type==0x06 || h.device_type==0x07)) ||
                   (h.version==0x7C && h.device_type==0x07);
  if (h.manufacturer != 0x4CAE || !model || h.ci != 0x7A)
    return Status::Unsupported;
  if(size<15) return Status::InvalidFrame;
  // Shared 5-bit OMS check also validates the encrypted-block count N.
  const auto security=wmbus_pure::tpl_security::inspect(frame,size);
  if(security.state!=wmbus_pure::tpl_security::State::NotEncrypted)
    return Status::Encrypted;
  // The only public clear on-air examples use exact TPL status 0x00
  // (OK) and 0x06 (ERROR POWER_LOW). Other states are not decoded as
  // healthy, even when the two measurement records look plausible.
  if(frame[12]!=0x00u && frame[12]!=0x06u)
    return Status::Unsupported;
  Reading decoded{};
  const auto result=decode_clear_application(frame+15,size-15,decoded);
  if(result!=Status::Ok) return result;
  decoded.tpl_status=frame[12];
  decoded.power_low=(frame[12]&0x04u)!=0;
  out=decoded;
  return Status::Ok;
}
} } // namespace xmq_portable::iperl
