#pragma once
// Sontex Supercom 587: conservative Mode-0 TPL standard BCD volume profile.
// Public original wmbusmeters supercom587.xmq (GPL-3.0-or-later).
// Not a generic vendor or AES decoder; no assumption NOKEY means clear RF.
#include <cstddef>
#include <cstdint>
#include "native_publication_gate.h"
#include "standard_dif_vif.h"
namespace xmq_portable { namespace supercom587 {
enum class Status:uint8_t {Ok,Invalid,WrongModel,Encrypted,BadRecords};
struct Reading {double total_m3{0};};
inline Status decode(const uint8_t *frame,size_t length,Reading &out) {
  out={};
  wmbus_pure::Header h{};
  if(wmbus_pure::parse_header(frame,length,h)!=wmbus_pure::HeaderStatus::Ok)
    return Status::Invalid;
  if(h.control!=0x44 || h.manufacturer!=0x4DEE ||
     h.version!=0x3C || (h.device_type!=0x06 && h.device_type!=0x07) ||
     h.ci!=0x7A)return Status::WrongModel;
  if(wmbus_pure::publication::inspect(frame,length,h.meter_id)!=
     wmbus_pure::publication::Decision::Allowed)return Status::Encrypted;
  // The public warm/cold water fixtures share exactly this long-profile
  // shape. Reject unrelated formats rather than decoding arbitrary bytes.
  if(length!=163 || frame[12]!=0 || frame[13]!=0 || frame[14]!=0)
    return Status::BadRecords;
  if(frame[15]!=0x0C || frame[16]!=0x13 ||
     frame[21]!=0x42 || frame[22]!=0x6C ||
     frame[25]!=0x4C || frame[26]!=0x13)
    return Status::BadRecords;
  standard::Record first[1]{};size_t count=0;
  if(standard::records(frame+15,6,first,1,count)!=standard::Status::Ok ||
     count!=1 || !first[0].numeric ||
     first[0].storage || first[0].vif!=0x13 ||
     first[0].value<0)return Status::BadRecords;
  double volume=0;
  if(!standard::to_canonical(0x13,first[0].value,volume) ||
     volume<0 || volume>100000000)return Status::BadRecords;
  out.total_m3=volume;return Status::Ok;
}
}} // namespace xmq_portable::supercom587
