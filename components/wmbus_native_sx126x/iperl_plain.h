#pragma once
// Portable, allocation-free iPERL CI=7A plaintext decoder.
// Public upstream fixtures; NOT an AES decryptor or an unchecked CSV reader.
#include <cstddef>
#include <cstdint>
#include "pure_frame_header.h"
#include "standard_dif_vif.h"
#include "xmq_standard_match_runtime.h"
#include "xmq_full_profile_runtime.h"
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
    double native_value=0;
    // The quantity, original XMQ name and VIF selectors are generated
    // from current iperl.xmq rather than embedded in this C++ decoder.
    if (xmq_standard::scaled(
          xmq_generated_standard::iperl::fields[0],r,native_value)) {
      if (total) return Status::Unsupported;
      out.total_m3=native_value;
      total=true;
    } else if (xmq_standard::scaled(
          xmq_generated_standard::iperl::fields[1],r,native_value)) {
      if (flow) return Status::Unsupported;
      out.max_flow_m3h=native_value;
      flow=true;
    }
  }
  if (!total || !flow) {
    out = {};
    return Status::MissingFields;
  }
  return Status::Ok;
}
// The one native RF admission/record/value/status interpreter is shared with
// CMA12w, LansenDW and Munia. The separate helper above is offline-only:
// upstream's first iPERL fixture is ALREADY DECRYPTED and must not be
// accidentally promoted as a clear on-air telegram.
inline const char *status_text(uint8_t tpl) {
  const auto &p=xmq_compiled_profiles::iperl::profile;
  for(size_t i=0;i<p.state_count;++i)
    if(p.states[i].tpl==tpl && p.states[i].error==0)
      return p.states[i].text;
  return nullptr;
}
inline Status decode(const uint8_t *frame,size_t length,Reading &out) {
  out={};
  full_xmq::Result value{};
  const auto status=full_xmq::decode(
    frame,length,xmq_compiled_profiles::iperl::profile,value);
  if(status!=full_xmq::Status::Ok) {
    if(status==full_xmq::Status::EncryptedOrUnknown)
      return Status::Encrypted;
    if(status==full_xmq::Status::Invalid)
      return Status::InvalidFrame;
    return Status::Unsupported;
  }
  Reading result{};
  result.total_m3=value.numeric[0];
  result.max_flow_m3h=value.numeric[1];
  result.tpl_status=value.tpl_status;
  result.power_low=(value.tpl_status&0x04u)!=0;
  out=result;
  return Status::Ok;
}
} } // namespace xmq_portable::iperl
