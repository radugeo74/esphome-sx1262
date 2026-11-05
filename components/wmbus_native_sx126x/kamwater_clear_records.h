#pragma once
// Bounded Kamstrup clear-application record decoder using shared DIF/VIF.
// Host-only stage: the upstream 8D20 ELL envelope may be encrypted or compact.
// This API does not unpack compact signatures, decrypt, select, or publish RF.
#include <cstddef>
#include <cstdint>
#include "standard_dif_vif.h"

namespace xmq_portable { namespace kamwater {
enum class Status : uint8_t { Ok, Invalid, Unsupported, MissingTotal, Ambiguous };
struct Reading {
  double total_m3{0};
  bool has_target{false};
  double target_m3{0};
  bool has_status{false};
  uint32_t status_bits{0};
};
inline Status decode_clear_records(const uint8_t *application, size_t size,
                                   Reading &out) {
  out={};
  standard::Record records[48]{};
  size_t count=0;
  if (standard::records(application,size,records,48,count)!=
      standard::Status::Ok)
    return Status::Invalid;
  bool has_total=false;
  for(size_t i=0;i<count;++i) {
    const auto &r=records[i];
    if (!r.numeric || r.function!=0 || r.tariff!=0 || r.subunit!=0)
      continue;
    if ((r.full_vif==0x7F20 || r.full_vif==0x7F23 ||
         r.full_vif==0x7F24) && r.storage==0) {
      if (out.has_status) return Status::Ambiguous;
      out.has_status=true;
      out.status_bits=static_cast<uint32_t>(r.raw64);
      continue;
    }
    if (r.vif<0x10 || r.vif>0x17 || r.vif_extension_count!=0)
      continue;
    double volume=0;
    if (!standard::to_canonical(r.vif,r.value,volume))
      return Status::Unsupported;
    if (r.storage==0) {
      if (has_total) return Status::Ambiguous;
      has_total=true;
      out.total_m3=volume;
    } else if (r.storage==1) {
      if (out.has_target) return Status::Ambiguous;
      out.has_target=true;
      out.target_m3=volume;
    }
  }
  if (!has_total) {out={};return Status::MissingTotal;}
  return Status::Ok;
}
}} // namespace xmq_portable::kamwater
