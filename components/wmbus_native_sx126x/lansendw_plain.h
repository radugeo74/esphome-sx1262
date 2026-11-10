#pragma once
// Lansen LAS/07/1D wireless door/window detector.
// Original public Lansen DW XMQ source and examples: GPL-3.0-or-later,
// Copyright (C) 2020-2022 Fredrik Öhrström.
// Independently implemented strict clear TPL/record decoder (NO AES).
// Caller MUST have independently verified complete DLL CRC.
#include <cstddef>
#include <cstdint>
#include "pure_frame_header.h"
#include "tpl_security_probe.h"
#include "lansenpu_plain.h"
#include "xmq_generated_clear_profiles.h"

namespace xmq_portable { namespace lansendw {
enum class Status : uint8_t {
  Ok, InvalidFrame, WrongModel, EncryptedOrUnknown, InvalidStatus,
  BadRecords, Duplicate, InvalidBcd, Missing
};
struct Reading {
  double a_counter{0};
  double b_counter{0};
  uint16_t digital_input{0};
  uint16_t error_flags{0};
  uint8_t tpl_status{0};
};
inline const char *state_text(uint16_t value) {
  if(value==0x0011u) return "CLOSED";
  if(value==0x0055u) return "OPEN";
  return nullptr;
}
inline const char *error_text(uint16_t error,uint8_t tpl_status) {
  // Only exact, independently verified fixture state combinations.
  if(error!=0x0001u) return nullptr;
  if(tpl_status==0x48u)
    return "ERROR_FLAGS_1 PERMANENT_ERROR TPL_STS_64";
  if(tpl_status==0x68u)
    return "ERROR_FLAGS_1 PERMANENT_ERROR TPL_STS_96";
  return nullptr;
}
// Only two source-proven TPL states are admitted: 0x48 and 0x68.
// Raw 0x40/0x60 are intentionally preserved as UNKNOWN, not assigned
// guessed alarm meanings. Only PERMANENT_ERROR (bit 0x08) is set.
inline bool confirmed_permanent_error(uint8_t tpl) {
  return tpl==0x48u || tpl==0x68u;
}
inline uint8_t unknown_tpl_bits(uint8_t tpl) {
  return static_cast<uint8_t>(tpl & 0x60u);
}
inline uint16_t unknown_error_bits(uint16_t error) {
  return error; // 0x0001 -> ERROR_FLAGS_1, not a defined alarm
}
inline uint16_t le16(const uint8_t *p) {
  return static_cast<uint16_t>(p[0]) |
         (static_cast<uint16_t>(p[1]) << 8);
}
inline Status decode(const uint8_t *frame,size_t length,Reading &out) {
  out={};
  wmbus_pure::Header h{};
  if(wmbus_pure::parse_header(frame,length,h)!=wmbus_pure::HeaderStatus::Ok)
    return Status::InvalidFrame;
  if(h.ci!=0x7A ||
     !xmq_generated_clear::match_model(h.manufacturer,h.version,h.device_type,
       xmq_generated_clear::lansendw::models,1))
    return Status::WrongModel;
  if(wmbus_pure::tpl_security::inspect(frame,length).state!=
     wmbus_pure::tpl_security::State::NotEncrypted)
    return Status::EncryptedOrUnknown;
  if(length<17) return Status::InvalidFrame;
  Reading tmp{};
  tmp.tpl_status=frame[12];
  if(tmp.tpl_status!=0x48 && tmp.tpl_status!=0x68)
    return Status::InvalidStatus;
  unsigned seen=0;
  size_t pos=15;
  while(pos<length) {
    if(frame[pos]==0x2F) {++pos;continue;}
    const int index=xmq_generated_clear::find_rule(frame+pos,length-pos,
        xmq_generated_clear::lansendw::rules,4);
    if(index<0) return Status::BadRecords;
    const unsigned slot=static_cast<unsigned>(index);
    const auto &rule=xmq_generated_clear::lansendw::rules[slot];
    const size_t prefix=rule.key_len, bytes=rule.value_len;
    if((seen&(1u<<slot))!=0) return Status::Duplicate;
    if(length-pos<prefix+bytes) return Status::BadRecords;
    const uint8_t *data=frame+pos+prefix;
    if(slot==0) {
      tmp.digital_input=le16(data);
      if(!state_text(tmp.digital_input)) return Status::InvalidStatus;
    } else if(slot==1) {
      tmp.error_flags=le16(data);
      if(tmp.error_flags!=0x0001) return Status::InvalidStatus;
    } else {
      double value=0;
      if(!lansenpu::bcd12(data,value)) return Status::InvalidBcd;
      if(slot==2) tmp.a_counter=value;
      else tmp.b_counter=value;
    }
    seen|=1u<<slot;
    pos+=prefix+bytes;
  }
  if(seen!=0x0Fu || !error_text(tmp.error_flags,tmp.tpl_status))
    return Status::Missing;
  out=tmp; // atomic assignment; no prior values on any rejection
  return Status::Ok;
}
}} // namespace xmq_portable::lansendw
