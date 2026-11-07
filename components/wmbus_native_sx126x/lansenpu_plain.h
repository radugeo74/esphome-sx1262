#pragma once
// Lansen Pulse Counter (lansenpu) — strict keyless OMS CI 7A.
// Based on wmbusmeters GPL-3.0-or-later lansenpu.xmq and its three
// public fixtures. No old radio/driver framework or decryption.
// Caller MUST have already verified the *complete* radio DLL CRC.
// Both counters are 12-digit BCD (6 bytes), exactly as XMQ declares.
// The header only parses; it NEVER publishes or registers sensors.
#include <cstddef>
#include <cstdint>
#include "pure_frame_header.h"
#include "tpl_security_probe.h"

namespace xmq_portable { namespace lansenpu {
enum class Status : uint8_t {
  Ok, InvalidFrame, WrongModel, EncryptedOrUnknown, BadRecords,
  DuplicateRecord, InvalidBcd, UnknownStatus, MissingCounter
};
struct Reading {
  bool has_a{false};
  bool has_b{false};
  double a_counter{0};
  double b_counter{0};
  uint8_t tpl_status{0};
};
inline bool bcd12(const uint8_t *bytes,double &result) {
  uint64_t decoded=0;
  for(int i=5;i>=0;--i) {
    const unsigned hi=bytes[i]>>4;
    const unsigned lo=bytes[i]&15u;
    if(hi>9 || lo>9) return false;
    decoded=decoded*100u+hi*10u+lo;
  }
  result=static_cast<double>(decoded); // all 12-digit BCD values exactly representable
  return true;
}
inline const char *status_text(uint8_t status) {
  // Explicit original fixtures only; no guessed TPL state interpretation.
  if(status==0x00) return "OK";
  if(status==0x04) return "POWER_LOW";
  if(status==0x4C) return "PERMANENT_ERROR POWER_LOW SABOTAGE_ENCLOSURE";
  return nullptr;
}
inline Status decode(const uint8_t *frame,size_t length,Reading &out) {
  out={};
  wmbus_pure::Header header{};
  if(wmbus_pure::parse_header(frame,length,header)!=
     wmbus_pure::HeaderStatus::Ok)
    return Status::InvalidFrame;
  if(header.manufacturer!=0x3033 ||
     !((header.version==0x14 && header.device_type==0x00) ||
       (header.version==0x1B && header.device_type==0x00) ||
       (header.version==0x0B && header.device_type==0x02)) ||
     header.ci!=0x7A)
    return Status::WrongModel;
  if(wmbus_pure::tpl_security::inspect(frame,length).state!=
     wmbus_pure::tpl_security::State::NotEncrypted)
    return Status::EncryptedOrUnknown;
  if(length<17) return Status::InvalidFrame;
  const uint8_t tpl_status=frame[12];
  if(!status_text(tpl_status)) return Status::UnknownStatus;
  size_t offset=15; // CI/ACC/status/config are 5 bytes including CI
  Reading candidate{};
  candidate.tpl_status=tpl_status;
  while(offset<length) {
    // Wireless M-Bus filler bytes between DIF/VIF records.
    if(frame[offset]==0x2F) {++offset;continue;}
    bool is_a=false;
    if(length-offset>=3 &&
       frame[offset]==0x0E && frame[offset+1]==0xFD &&
       frame[offset+2]==0x3A) {
      is_a=true;
      offset+=3;
      if(candidate.has_a) return Status::DuplicateRecord;
    } else if(length-offset>=4 &&
              frame[offset]==0x8E && frame[offset+1]==0x40 &&
              frame[offset+2]==0xFD && frame[offset+3]==0x3A) {
      offset+=4;
      if(candidate.has_b) return Status::DuplicateRecord;
    } else {
      return Status::BadRecords; // no skipping unknown records or unknown VIFE
    }
    if(length-offset<6) return Status::BadRecords;
    double count=0;
    if(!bcd12(frame+offset,count)) return Status::InvalidBcd;
    offset+=6;
    if(is_a) {candidate.has_a=true;candidate.a_counter=count;}
    else {candidate.has_b=true;candidate.b_counter=count;}
  }
  if(!candidate.has_a && !candidate.has_b)
    return Status::MissingCounter;
  out=candidate; // no stale/partial values on any invalid input
  return Status::Ok;
}
}} // namespace xmq_portable::lansenpu
