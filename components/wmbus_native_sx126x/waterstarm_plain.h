#pragma once
// DWZ Waterstar M: fixed 151-byte public TPL Mode-0 archival profile.
// Upstream waterstarm.xmq, GPL-3.0-or-later. All other revisions and
// NOKEY-labelled Mode-5/ELL examples are rejected here, not decrypted.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "native_publication_gate.h"
#include "standard_dif_vif.h"
namespace xmq_portable { namespace waterstarm {
enum class Status:uint8_t {Ok,Invalid,WrongModel,NotClear,BadRecords,Missing,Duplicate};
struct Reading {double total_m3{0},consumption_at_set_date_m3{0};};
inline Status decode(const uint8_t *frame,size_t n,Reading &out) {
 out={};wmbus_pure::Header h{};
 if(wmbus_pure::parse_header(frame,n,h)!=wmbus_pure::HeaderStatus::Ok)
   return Status::Invalid;
 if(h.control!=0x44 || h.manufacturer!=0x12FA ||
    h.version!=0x00 || (h.device_type!=0x06 && h.device_type!=0x07) ||
    h.ci!=0x7A)return Status::WrongModel;
 if(wmbus_pure::publication::inspect(frame,n,h.meter_id)!=
    wmbus_pure::publication::Decision::Allowed)return Status::NotClear;
 // Only the two public DWZ/00/06-07 Mode-0 151-byte radio profiles.
 if(n!=151 || frame[12]!=0 || frame[13]!=0 || frame[14]!=0x20 ||
    frame[15]!=0x04 || frame[16]!=0x6D ||
    frame[21]!=0x04 || frame[22]!=0x13)
   return Status::BadRecords;
 standard::Record rows[64]{};size_t count=0;
 if(standard::records(frame+15,n-15,rows,64,count)!=
    standard::Status::Ok)return Status::BadRecords;
 Reading next{};unsigned hits=0;
 for(size_t i=0;i<count;++i) {
   const auto&r=rows[i];
   if(!r.numeric || r.vif!=0x13 || r.storage>1 || r.function ||
      r.tariff || r.subunit || r.vif_extension_count || r.modifier_count)
     continue;
   if(hits&(1u<<r.storage))return Status::Duplicate;
   double v=0;
   if(!standard::to_canonical(0x13,r.value,v) || !std::isfinite(v) ||
      v<0 || v>100000000)return Status::BadRecords;
   hits|=1u<<r.storage;
   if(r.storage==0)next.total_m3=v;
   else next.consumption_at_set_date_m3=v;
 }
 if(hits!=3)return Status::Missing;
 if(next.consumption_at_set_date_m3>next.total_m3)return Status::BadRecords;
 out=next;return Status::Ok;
}
}} // namespace xmq_portable::waterstarm
