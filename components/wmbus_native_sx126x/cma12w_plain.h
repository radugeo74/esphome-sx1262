#pragma once
// Elvaco CMA12w ELV/20/1B and ELV/1F/1B room sensor.
// Original XMQ source and two NOKEY examples: GPL-3.0-or-later,
// Copyright (C) 2020-2022 Fredrik Öhrström.
// Strict native clear short-TPL parser, NO AES or legacy meter stack.
// Caller MUST independently verify the full DLL CRC.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include "pure_frame_header.h"
#include "tpl_security_probe.h"

namespace xmq_portable { namespace cma12w {
enum class Status : uint8_t {
  Ok, InvalidFrame, WrongModel, EncryptedOrUnknown, InvalidStatus,
  BadRecords, Duplicate, Missing, SoftwareVersion
};
struct Reading {
  double current_temperature_c{0};
  double average_temperature_1h_c{0};
  uint16_t battery_bits{0};
  char software_version[8]{};
};
inline uint16_t le16(const uint8_t *p) {
  return static_cast<uint16_t>(p[0]) |
         (static_cast<uint16_t>(p[1])<<8);
}
inline double signed_hundredths(const uint8_t *p) {
  const uint16_t bits=le16(p);
  const int32_t raw=(bits&0x8000u) ? static_cast<int32_t>(bits)-65536 :
                                   static_cast<int32_t>(bits);
  return static_cast<double>(raw)/100.0;
}
inline Status decode(const uint8_t *frame,size_t length,Reading &out) {
  out={};
  wmbus_pure::Header h{};
  if(wmbus_pure::parse_header(frame,length,h)!=wmbus_pure::HeaderStatus::Ok)
    return Status::InvalidFrame;
  if(h.manufacturer!=0x1596 || h.device_type!=0x1B ||
     (h.version!=0x20 && h.version!=0x1F) || h.ci!=0x7A)
    return Status::WrongModel;
  if(wmbus_pure::tpl_security::inspect(frame,length).state!=
     wmbus_pure::tpl_security::State::NotEncrypted)
    return Status::EncryptedOrUnknown;
  if(length<17) return Status::InvalidFrame;
  if(frame[12]!=0x00) return Status::InvalidStatus; // only documented OK
  Reading tmp{};
  unsigned seen=0;
  size_t pos=15;
  while(pos<length) {
    if(frame[pos]==0x2F) {++pos;continue;}
    // Standard manufacturer-specific trailer is permitted only AFTER
    // the complete software-version record, at the very end.
    if(frame[pos]==0x0F && pos+1==length && seen==0x0Fu) {
      ++pos;
      continue;
    }
    unsigned slot=4;
    size_t prefix=0,bytes=0;
    if(length-pos>=2 && frame[pos]==0x02 && frame[pos+1]==0x65) {
      slot=0;prefix=2;bytes=2;
    } else if(length-pos>=2 && frame[pos]==0x42 && frame[pos+1]==0x65) {
      slot=1;prefix=2;bytes=2;
    } else if(length-pos>=3 && frame[pos]==0x02 &&
              frame[pos+1]==0xFD && frame[pos+2]==0x1B) {
      slot=2;prefix=3;bytes=2;
    } else if(length-pos>=4 && frame[pos]==0x0D &&
              frame[pos+1]==0xFD && frame[pos+2]==0x0F &&
              frame[pos+3]==0x05) {
      slot=3;prefix=4;bytes=5;
    } else return Status::BadRecords;
    if((seen&(1u<<slot))!=0) return Status::Duplicate;
    if(length-pos<prefix+bytes) return Status::BadRecords;
    const uint8_t *data=frame+pos+prefix;
    if(slot==0) tmp.current_temperature_c=signed_hundredths(data);
    else if(slot==1) tmp.average_temperature_1h_c=signed_hundredths(data);
    else if(slot==2) tmp.battery_bits=le16(data);
    else {
      // The original XMQ fixture stores the dotted ASCII software
      // version in reverse component order: 0.0.4 -> 4.0.0.
      if(data[1]!='.' || data[3]!='.' ||
         data[0]<'0' || data[0]>'9' ||
         data[2]<'0' || data[2]>'9' ||
         data[4]<'0' || data[4]>'9')
        return Status::SoftwareVersion;
      std::snprintf(tmp.software_version,sizeof(tmp.software_version),
                    "%c.%c.%c",data[4],data[2],data[0]);
    }
    seen|=1u<<slot;
    pos+=prefix+bytes;
  }
  if(seen!=0x0Fu) return Status::Missing; // require complete documented profile
  out=tmp;
  return Status::Ok;
}
}} // namespace xmq_portable::cma12w
