#pragma once
// Generic bounded expansion of a VERIFIED compact DIF/VIF profile.
// Does not infer format from data, discover CRC signatures, or decrypt.
// Callers must provide the correct, prevalidated profile for this telegram.
#include <cstddef>
#include <cstdint>
#include "standard_dif_vif.h"
namespace xmq_portable { namespace compact {
enum class Status : uint8_t { Ok, Invalid, Unsupported, Truncated, Trailing, Capacity };
inline Status expand(const uint8_t *profile, size_t profile_size,
                     const uint8_t *payload, size_t payload_size,
                     uint8_t *out, size_t capacity, size_t &used) {
  used=0;
  if (!profile || !payload || !out || !profile_size || !payload_size)
    return Status::Invalid;
  size_t p=0, v=0, w=0;
  while (p<profile_size) {
    const size_t start=p;
    const uint8_t original_dif=profile[p++];
    if (original_dif==0x2F || original_dif==0x0F ||
        original_dif==0x1F || (original_dif&15u)==13)
      return Status::Unsupported;
    uint8_t dif=original_dif;
    unsigned dif_extensions=0;
    while (dif & 0x80u) {
      if (p>=profile_size || ++dif_extensions>8) return Status::Unsupported;
      dif=profile[p++];
    }
    if (p>=profile_size) return Status::Truncated;
    uint8_t vif=profile[p++];
    // A VIF=7C text label has variable length; fail rather than guessing.
    if ((vif&0x7Fu)==0x7C) return Status::Unsupported;
    unsigned vif_extensions=0;
    while (vif&0x80u) {
      if (p>=profile_size || ++vif_extensions>10) return Status::Unsupported;
      vif=profile[p++];
    }
    const size_t data_length=standard::format_length(original_dif&15u);
    if (data_length==SIZE_MAX) return Status::Unsupported;
    if (data_length>payload_size-v) return Status::Truncated;
    const size_t header_length=p-start;
    if (header_length>capacity-w || data_length>capacity-w-header_length)
      return Status::Capacity;
    for(size_t j=0;j<header_length;++j) out[w++]=profile[start+j];
    for(size_t j=0;j<data_length;++j) out[w++]=payload[v++];
  }
  if (v!=payload_size) return Status::Trailing;
  used=w;
  return Status::Ok;
}
}} // namespace xmq_portable::compact
