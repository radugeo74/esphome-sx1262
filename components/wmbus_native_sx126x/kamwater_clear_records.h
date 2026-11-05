#pragma once
// Bounded Kamstrup clear-application record decoder using shared DIF/VIF.
// Host-only stage: the upstream 8D20 ELL envelope may be encrypted or compact.
// This API does not unpack compact signatures, decrypt, select, or publish RF.
#include <cstddef>
#include <cstdint>
#include "standard_dif_vif.h"
#include "compact_dif_vif.h"

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
  Reading parsed{};
  bool has_total=false;
  for(size_t i=0;i<count;++i) {
    const auto &r=records[i];
    if (!r.numeric || r.function!=0 || r.tariff!=0 || r.subunit!=0)
      continue;
    if ((r.full_vif==0x7F20 || r.full_vif==0x7F23 ||
         r.full_vif==0x7F24) && r.storage==0) {
      if (parsed.has_status) return Status::Ambiguous;
      parsed.has_status=true;
      parsed.status_bits=static_cast<uint32_t>(r.raw64);
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
      parsed.total_m3=volume;
    } else if (r.storage==1) {
      if (parsed.has_target) return Status::Ambiguous;
      parsed.has_target=true;
      parsed.target_m3=volume;
    }
  }
  if (!has_total) return Status::MissingTotal;
  out=parsed;
  return Status::Ok;
}
// Host-only acceptance of the first explicitly known Kamstrup compact
// template from original kamwater.xmq. This function MUST NOT be wired to
// live RF without validating the telegram's compact-format signature and
// the ELL security state first. Payload alone is not sufficient.
inline Status decode_known_clear_compact_profile_0(const uint8_t *payload,
                                                     size_t length,
                                                     Reading &out) {
  out={};
  static constexpr uint8_t profile[] = {
    0x02,0xFF,0x20, 0x04,0x13, 0x44,0x13, 0x61,0x5B, 0x61,0x67
  };
  uint8_t expanded[64]{};
  size_t expanded_length=0;
  if (compact::expand(profile,sizeof(profile),payload,length,
                      expanded,sizeof(expanded),expanded_length)!=
      compact::Status::Ok)
    return Status::Invalid;
  return decode_clear_records(expanded,expanded_length,out);
}
// Second original Kamwater compact template (MULTICAL21): status, total,
// max flow and temperature records. Accept ONLY application data after a
// caller has independently verified the format signature and ELL security.
// The official compact fixture contains preceding bytes F4307912C40DFF;
// they are not measurements and must never be silently consumed here.
inline Status decode_known_clear_compact_profile_1(const uint8_t *payload,
                                                     size_t length,
                                                     Reading &out) {
  out={};
  static constexpr uint8_t profile[] = {
    0x02,0xFF,0x20, 0x04,0x13,
    0x92,0x01,0x3B, 0xA1,0x01,0x5B,
    0x81,0x01,0xE7,0xFF,0x0F
  };
  uint8_t expanded[64]{};
  size_t expanded_length=0;
  if (compact::expand(profile,sizeof(profile),payload,length,
                      expanded,sizeof(expanded),expanded_length)!=
      compact::Status::Ok)
    return Status::Invalid;
  return decode_clear_records(expanded,expanded_length,out);
}
}} // namespace xmq_portable::kamwater
