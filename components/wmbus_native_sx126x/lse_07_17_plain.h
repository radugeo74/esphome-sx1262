#pragma once
// LSE water LSE/18/06 and LSE/D8/07 clear short-TPL profiles.
// Source: wmbusmeters/drivers/src/lse_07_17.xmq GPL-3.0-or-later.
// Both observed versions are present in the upstream detect.mvt table;
// configured AUTO may resolve them only by exact MVT + bound DLL ID.
// Only four bounded source profiles, no generic LSE variants or key bypass.
#include <cstddef>
#include <cstdint>
#include <cmath>
#include "native_publication_gate.h"
#include "standard_dif_vif.h"
namespace xmq_portable { namespace lse_07_17 {
enum class Status : uint8_t {Ok, Invalid, WrongModel, NotClear, BadRecords, Missing, Duplicate};
struct Reading {
 bool has_total{false},has_due{false},has_set8{false};
 double total_m3{0},due_m3{0},consumption_at_set_date_8_m3{0};
};
inline Status decode(const uint8_t *frame,size_t length,Reading &out) {
 out={};wmbus_pure::Header h{};
 if(wmbus_pure::parse_header(frame,length,h)!=wmbus_pure::HeaderStatus::Ok)
   return Status::Invalid;
 if(h.control!=0x44 || h.manufacturer!=0x3265 || h.ci!=0x7A ||
    !((h.version==0x18 && h.device_type==0x06) ||
      (h.version==0xD8 && h.device_type==0x07)))
   return Status::WrongModel;
 if(wmbus_pure::publication::inspect(frame,length,h.meter_id)!=
    wmbus_pure::publication::Decision::Allowed)return Status::NotClear;
 // Original 37/43-byte storage-8 forms and two 46-byte total/due forms.
 const bool archive=h.version==0x18 && (length==37 || length==43);
 const bool standard_form=length==46 &&
    ((h.version==0x18 && h.device_type==0x06) ||
     (h.version==0xD8 && h.device_type==0x07));
 if(!archive && !standard_form)return Status::BadRecords;
 if(frame[13]!=0 || frame[14]!=0 ||
    !(frame[12]==0 || (archive && length==43 && frame[12]==0x80)))
   return Status::BadRecords;
 if(archive) {
    if(frame[15]!=0x8C || frame[16]!=0x04 || frame[17]!=0x13 ||
       frame[22]!=0x82 || frame[23]!=0x04 || frame[24]!=0x6C)
       return Status::BadRecords;
 } else {
   const bool short_format=frame[15]==0x0C && frame[16]==0x13;
   const bool datetime_format=frame[15]==0x04 && frame[16]==0x6D;
   if(h.version==0x18 && !short_format)return Status::BadRecords;
   if(h.version==0xD8 && !datetime_format)return Status::BadRecords;
 }
 standard::Record rec[24]{};size_t count=0;
 if(standard::records(frame+15,length-15,rec,24,count)!=standard::Status::Ok)
   return Status::BadRecords;
 Reading next{};
 for(size_t i=0;i<count;++i) {
   const auto&r=rec[i];
   if(!r.numeric || r.function || r.tariff || r.subunit ||
      r.vif_extension_count || r.modifier_count || r.vif!=0x13)continue;
   double v=0;
   if(!standard::to_canonical(0x13,r.value,v) ||
      !std::isfinite(v) || v<0 || v>100000000)return Status::BadRecords;
   if(r.storage==0) {
      if(next.has_total)return Status::Duplicate;
      next.has_total=true;next.total_m3=v;
   } else if(r.storage==1) {
      if(next.has_due)return Status::Duplicate;
      next.has_due=true;next.due_m3=v;
   } else if(r.storage==8) {
      if(next.has_set8)return Status::Duplicate;
      next.has_set8=true;next.consumption_at_set_date_8_m3=v;
   }
 }
 if(archive) {
   if(!next.has_set8 || next.has_total || next.has_due)return Status::Missing;
 } else {
   if(!next.has_total || !next.has_due || next.has_set8)return Status::Missing;
   if(next.due_m3>next.total_m3)return Status::BadRecords;
 }
 out=next;return Status::Ok;
}
}} // namespace xmq_portable::lse_07_17
