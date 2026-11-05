#pragma once
// Zenner Minomess upstream IXML prefix contract, portable and allocation-free.
// Host-side clear-application decoder ONLY until the full RF/TPL path has
// passed official fixtures, including DLL-versus-TPL identity and security.
// Upstream minomess.xmq: 8C0413 claims BCD, but actual target is binary
// uint32 litres; FFFFFFFF and 80000000 are missing/null sentinels.
#include <cstddef>
#include <cstdint>
#include "standard_dif_vif.h"

#include "pure_frame_header.h"
#include "tpl_security_probe.h"
#include "native_identity_probe.h"


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
  uint16_t status_bits{0};
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
  if (!data || n<22) return Status::Truncated;
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
// Validate the complete remaining standard record sequence after the special
// Minomess prefix. The 8D049313 record is an LVAR opaque monthly-history
// blob, not a list of measurements. We validate its declared length without
// interpreting its undocumented contents. The 02FD17 status is mandatory.
inline Status decode_clear_application(const uint8_t *data, size_t n,
                                       Reading &out) {
  out={};
  Reading parsed{};
  const auto prefix=decode_clear_prefix(data,n,parsed);
  if (prefix!=Status::Ok) return prefix;
  size_t i=22;
  if (n<i+5 || data[i]!=0x8D || data[i+1]!=0x04 ||
      data[i+2]!=0x93 || data[i+3]!=0x13)
    return Status::MissingFields;
  // LVAR is byte count for opaque text. The actual upstream driver
  // uses length 0x2C (44 bytes) for monthly history; never overread.
  const size_t history_length=data[i+4];
  if (history_length!=44 || history_length>n-(i+5))
    return Status::Unsupported;
  i+=5+history_length;
  if (n-i<5 || data[i]!=0x02 || data[i+1]!=0xFD ||
      data[i+2]!=0x17)
    return Status::MissingFields;
  parsed.status_bits=static_cast<uint16_t>(data[i+3]) |
                     (static_cast<uint16_t>(data[i+4])<<8);
  i+=5;
  // The wired M-Bus upstream fixtures may carry a gateway RSSI and
  // actuality records. Decode their shape with the shared bounded parser,
  // without interpreting them as meter-owned readings.
  standard::Record rest[8]{};
  size_t rest_count=0;
  if (standard::records(data+i,n-i,rest,8,rest_count)!=
      standard::Status::Ok)
    return Status::Unsupported;
  out=parsed;
  return Status::Ok;
}
// Read-only wrapper for a complete, CRC-stripped wireless frame.
// The official wired M-Bus 0x68 frames use a different envelope and are not
// accepted here. This host-stage decoder DOES NOT authorize sensor output.
// Only a clear short/long TPL may reach the existing application parser.
inline Status decode_clear_wireless_frame(const uint8_t *frame, size_t n,
                                          Reading &out) {
  out = {};
  wmbus_pure::Header header{};
  if (wmbus_pure::parse_header(frame,n,header)!=wmbus_pure::HeaderStatus::Ok)
    return Status::Unsupported;
  // The outer DLL can belong to a forwarding radio module. Validate the
  // actual embedded TPL signature instead; this is not an authorization
  // to publish under an unmatched DLL ID.
  const auto security=wmbus_pure::tpl_security::inspect(frame,n);
  if (security.state!=wmbus_pure::tpl_security::State::NotEncrypted)
    return Status::Unsupported;
  const auto identity=wmbus_pure::identity::inspect(frame,n);
  if (identity.status!=wmbus_pure::identity::Status::Ok)
    return Status::Unsupported;
  const auto &meter=(identity.has_tpl ? identity.tpl : identity.dll);
  if (meter.manufacturer!=0x6A49 ||
      !((meter.version==0x00 && meter.device_type==0x07) ||
        (meter.version==0x01 &&
         (meter.device_type==0x06 || meter.device_type==0x16))))
    return Status::Unsupported;
  if (header.ci!=0x72 && header.ci!=0x7A)
    return Status::Unsupported;
  const size_t offset=identity.application_offset;
  if (offset>=n) return Status::Truncated;
  size_t start=offset;
  while (start<n && frame[start]==0x2F) ++start;
  if (start>=n) return Status::Truncated;
  // Unverified proprietary remainder cannot be reused as another reading.
  return decode_clear_application(frame+start,n-start,out);
}
}}  // namespace xmq_portable::minomess
