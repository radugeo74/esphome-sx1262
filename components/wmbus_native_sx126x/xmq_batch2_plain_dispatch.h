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

namespace xmq_portable { namespace batch2_plain {
inline bool is_explicit_driver(const char *name) {
  return name &&
     (std::strcmp(name,"minomess")==0 ||
      std::strcmp(name,"kamwater")==0 ||
      std::strcmp(name,"apator162")==0 ||
      std::strcmp(name,"lansenpu")==0);
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
