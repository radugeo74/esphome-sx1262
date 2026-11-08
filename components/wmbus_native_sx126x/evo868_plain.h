#pragma once
// Maddalena EVO868: verified Mode-0 TPL, bounded standard consumption subset.
// Public original wmbusmeters evo868.xmq (GPL-3.0-or-later).
// Receiver is responsible for full DLL CRC and ID binding before calling.
#include <cstddef>
#include <cstdint>
#include <cmath>
#include "native_publication_gate.h"
#include "standard_dif_vif.h"
namespace xmq_portable { namespace evo868 {
enum class Status:uint8_t {Ok,Invalid,WrongModel,Encrypted,BadRecords,Duplicate,Missing};
struct Reading {
  double total_m3{0};
  double consumption_at_set_date_m3{0};
};
inline Status decode(const uint8_t *frame,size_t length,Reading &out) {
  out={};
  wmbus_pure::Header h{};
  if(wmbus_pure::parse_header(frame,length,h)!=wmbus_pure::HeaderStatus::Ok)
    return Status::Invalid;
  if(h.control!=0x44 || h.manufacturer!=0x3424 || h.version!=0x50 ||
     (h.device_type!=0x06 && h.device_type!=0x07 &&
      h.device_type!=0x16) || h.ci!=0x7A)return Status::WrongModel;
  if(wmbus_pure::publication::inspect(frame,length,h.meter_id)!=
     wmbus_pure::publication::Decision::Allowed)return Status::Encrypted;
  // These two observed public variants have full, distinct 173/175-byte
  // bodies, complete standard TPL header and no status fault.
  if((length!=173 && length!=175) || frame[12]!=0 ||
     frame[13]!=0 || frame[14]!=0x20)return Status::BadRecords;
  size_t start=15;
  while(start<length && frame[start]==0x2f)++start;
  if(start+6>length || frame[start]!=0x04 || frame[start+1]!=0x13)
    return Status::BadRecords;
  standard::Record first[1]{};size_t first_count=0;
  if(standard::records(frame+start,6,first,1,first_count)!=
     standard::Status::Ok || first_count!=1 || !first[0].numeric ||
     first[0].storage || first[0].vif!=0x13)return Status::BadRecords;
  double volume=0;
  if(!standard::to_canonical(0x13,first[0].value,volume) ||
     volume<0 || volume>100000000)return Status::BadRecords;
  // Standard DIF 0x44 / VIF 0x13 means stored previous billing reading.
  // Locate exactly one such record, directly followed by the standard
  // 0x42/0x6C billing-date record in the two upstream examples.
  size_t target=0;unsigned matches=0;
  for(size_t i=start+6;i+14<=length;++i) {
    if(frame[i]==0x44 && frame[i+1]==0x13 &&
       frame[i+6]==0x42 && frame[i+7]==0x6c) {
      target=i;++matches;
    }
  }
  if(matches!=1)return matches?Status::Duplicate:Status::Missing;
  standard::Record rec[1]{};size_t count=0;
  if(standard::records(frame+target,6,rec,1,count)!=standard::Status::Ok ||
     count!=1 || !rec[0].numeric || rec[0].storage!=1 ||
     rec[0].vif!=0x13)return Status::BadRecords;
  double previous=0;
  if(!standard::to_canonical(0x13,rec[0].value,previous) ||
     previous<0 || previous>volume)return Status::BadRecords;
  out={volume,previous};return Status::Ok;
}
}} // namespace xmq_portable::evo868
