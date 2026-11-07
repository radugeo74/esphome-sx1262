#pragma once
// Explicit no-key native decoder for *actually unencrypted* W-MBus RF.
// Input MUST be complete DLL CRC-verified, CRC-stripped by SX126x.
// Never infer cleartext from NOKEY upstream fixture metadata: original
// wmbusmeters fixtures can contain applications already decrypted upstream.
// No fallback to this decoder after a failed keyed decode; do not AUTO.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "xmq_decode_catalog.h"
#include "xmq_auto_dispatch.h"

namespace xmq_portable { namespace batch2_plain {
inline bool is_explicit_driver(const char *name) {
  return name &&
     (std::strcmp(name,"minomess")==0 ||
      std::strcmp(name,"kamwater")==0 ||
      std::strcmp(name,"apator162")==0 ||
      std::strcmp(name,"lansenpu")==0);
}
// Safe auto routing only for configured Lansen. All three prior batch-2
// drivers remain explicit-only, regardless of their MVT metadata.
// No auto-decryption, unknown-meter registration or sensor publication.
inline const char *select_configured_driver(
    const uint8_t *frame,size_t length,const char *meter_id,
    const char *dll_id,const char *requested,
    const wmbus_pure::DriverSignature *signatures,size_t count) {
  if(!frame || !meter_id || !dll_id || !requested) return nullptr;
  wmbus_pure::Header header{};
  if(wmbus_pure::parse_header(frame,length,header)!=
     wmbus_pure::HeaderStatus::Ok ||
     wmbus_pure::match_meter_id(header,dll_id)!=
     wmbus_pure::MatchStatus::Match)
    return nullptr;
  if(std::strcmp(requested,"auto")!=0)
    return is_explicit_driver(requested) ? requested : nullptr;
  // A radio converter may not select Lansen AUTO for a different TPL ID.
  if(std::strcmp(meter_id,dll_id)!=0) return nullptr;
  const auto resolved=wmbus_pure::auto_driver::resolve(
      requested,signatures,count,header.manufacturer,header.version,
      header.device_type);
  return resolved.status==wmbus_pure::auto_driver::Status::Ready &&
         resolved.driver && std::strcmp(resolved.driver,"lansenpu")==0
             ? resolved.driver : nullptr;
}
inline PureDecodeStatus decode_configured(
    const uint8_t *frame,size_t length,const char *meter_id,
    const char *dll_id,const char *driver,Capture *captures,
    size_t capture_capacity,DecodedPayload &out) {
  out.value_count=0;
  out.date_count=0;
  if(!frame || !meter_id || !dll_id || !is_explicit_driver(driver))
    return PureDecodeStatus::UnknownDriver;
  return decode_configured_plain_frame(
      frame,length,meter_id,driver,captures,capture_capacity,out,dll_id);
}
}} // namespace xmq_portable::batch2_plain
