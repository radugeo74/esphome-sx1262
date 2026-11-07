#pragma once
// Host-only explicit supplied-clear reference for the original public
// Integra Topas ESKR NOKEY/XMQ examples. NOT a radio security bypass.
// This file is never included by components/wmbus_native_sx126x or
// compiled into the Heltec receiver. Mode-5 on-air packets still require
// AES and configured secure_meters.
#include <cstddef>
#include <cstdint>
#include "components/wmbus_native_sx126x/pure_frame_header.h"
#include "components/wmbus_native_sx126x/pure_meter_match.h"
#include "components/wmbus_native_sx126x/topaseskr_plain.h"

namespace xmq_portable { namespace topaseskr { namespace supplied_clear {
enum class Status : uint8_t {
  Ok, InvalidFrame, WrongMeter, WrongModel, NotSuppliedClear, InvalidRecords
};

// Only call on an explicitly supplied already-clear *offline* frame.
// The caller is responsible for provenance: a 2F2F marker is NOT proof
// that data observed from live RF has been decrypted.
inline Status decode(const uint8_t *frame, size_t length,
                     const char *expected_id, Reading &out) {
  out={};
  wmbus_pure::Header h{};
  if(wmbus_pure::parse_header(frame,length,h)!=
     wmbus_pure::HeaderStatus::Ok) return Status::InvalidFrame;
  if(wmbus_pure::match_meter_id(h,expected_id)!=
     wmbus_pure::MatchStatus::Match) return Status::WrongMeter;
  if(h.manufacturer!=0x05B4 || h.version!=0xF1 ||
     (h.device_type!=0x06 && h.device_type!=0x07) || h.ci!=0x7A)
    return Status::WrongModel;
  // Four AES-CBC blocks are indicated by the TPL header, even though
  // the upstream NOKEY example supplies clear bytes at that position.
  if(length!=79 || frame[13]!=0x40 || frame[14]!=0x05 ||
     frame[15]!=0x2F || frame[16]!=0x2F)
    return Status::NotSuppliedClear;
  Reading reading{};
  if(decode_clear_application(frame+17,length-17,reading)!=topaseskr::Status::Ok)
    return Status::InvalidRecords;
  out=reading;
  return Status::Ok;
}
}}} // namespace xmq_portable::topaseskr::supplied_clear
