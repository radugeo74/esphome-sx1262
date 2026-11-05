#pragma once
// Zenner Minomess upstream IXML prefix contract, portable and allocation-free.
// Host-side clear-application decoder ONLY until the full RF/TPL path has
// passed official fixtures, including DLL-versus-TPL identity and security.
// Upstream minomess.xmq: 8C0413 claims BCD, but actual target is binary
// uint32 litres; FFFFFFFF and 80000000 are missing/null sentinels.
#include <cstddef>
#include <cstdint>
#include "standard_dif_vif.h"

namespace xmq_portable { namespace minomess {
enum class Status : uint8_t { Ok, Truncated, Unsupported, InvalidBcd,
                              InvalidDate, MissingFields };
struct Reading {
  double total_m3{0};
  bool has_target{false};
  double target_m3{0};
  bool has_meter_date{false};
  bool has_target_date{false};
  standard::Calendar meter_date{};
  standard::Calendar target_date{};
};
inline uint32_t u32le(const uint8_t *p) {
  return uint32_t(p[0]) | (uint32_t(p[1])<<8) |
         (uint32_t(p[2])<<16) | (uint32_t(p[3])<<24);
}
inline bool bcd8(const uint8_t *p, uint32_t &out) {
  out=0; uint32_t mul=1;
  for (int i=0;i<4;++i) {
    const unsigned low=p[i]&15u, high=p[i]>>4;
    if (low>9 || high>9) return false;
    out+=(low+high*10u)*mul; mul*=100u;
  }
  return true;
}
// Intentional limited prefix parser: it does not interpret proprietary
// 8D0493 monthly history tails. Never use it to claim full driver parity.
// Requires already validated clear application with consecutive fields:
// 0C13 total BCD litres, 026C meter date, 82046C target date,
// 8C0413 target binary litres or upstream null sentinel.
inline Status decode_clear_prefix(const uint8_t *data, size_t n, Reading &out) {
  out={};
  if (!data || n<23) return Status::Truncated;
  if (data[0]!=0x0C || data[1]!=0x13 ||
      data[6]!=0x02 || data[7]!=0x6C ||
      data[10]!=0x82 || data[11]!=0x04 || data[12]!=0x6C ||
      data[15]!=0x8C || data[16]!=0x04 || data[17]!=0x13)
    return Status::Unsupported;
  uint32_t litres=0;
  if (!bcd8(data+2,litres)) return Status::InvalidBcd;
  standard::Calendar meter{}, target{};
  if (!standard::decode_calendar(uint32_t(data[8]) | (uint32_t(data[9])<<8),
                                 false,meter) ||
      !standard::decode_calendar(uint32_t(data[13]) | (uint32_t(data[14])<<8),
                                 false,target))
    return Status::InvalidDate;
  const uint32_t target_raw=u32le(data+18);
  out.total_m3=litres/1000.0;
  out.meter_date=meter;
  out.target_date=target;
  out.has_meter_date=true;
  out.has_target_date=true;
  // Original minomess.xmq declares -2147483.648 as a null target.
  // FFFFFFFF means 'not set', and must never become a numeric reading.
  if (target_raw!=0xFFFFFFFFu && target_raw!=0x80000000u) {
    if (target_raw>0x7FFFFFFFu) {out={};return Status::Unsupported;}
    out.target_m3=target_raw/1000.0;
    out.has_target=true;
  }
  return Status::Ok;
}
}}  // namespace xmq_portable::minomess
