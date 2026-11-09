#pragma once
// Landis+Gyr QHeat 5.5 US (LUG/07-0A/04) strict short TPL clear subset.
// Source wmbusmeters/drivers/src/qheat_55_us.xmq (GPL-3.0-or-later).
// Four upstream Mode-0 fixtures, including a recorded F0 error.
// Numeric reading does NOT imply a healthy meter; status interpretation
// needs separate field-level evidence, never label all frames "OK".
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "standard_dif_vif.h"
#include "native_publication_gate.h"
namespace xmq_portable { namespace qheat_55_us {
enum class Status:uint8_t {Ok,Invalid,WrongModel,NotClear,BadRecords,Missing,Duplicate};
struct Reading {
 double total_energy_consumption_kwh{0};
 double key_datetime_kwh{0};
 bool has_total_m3{false};
 double total_m3{0};
};
inline Status decode(const uint8_t *frame,size_t n,Reading &out) {
 out={};wmbus_pure::Header h{};
 if(wmbus_pure::parse_header(frame,n,h)!=wmbus_pure::HeaderStatus::Ok)
   return Status::Invalid;
 if(h.control!=0x44 || h.manufacturer!=0x32A7 ||
    (h.version!=0x07 && h.version!=0x0A) ||
    h.device_type!=0x04 || h.ci!=0x7A)return Status::WrongModel;
 if(wmbus_pure::publication::inspect(frame,n,h.meter_id)!=
    wmbus_pure::publication::Decision::Allowed)return Status::NotClear;
 // Only upstream observed 151/70/57-byte families with clear short TPL.
 // Frame[12] is a status byte and can be nonzero (F0 error fixture).
 if((n!=151 && n!=70 && n!=57) ||
    frame[13]!=0 || frame[14]!=0x20)
   return Status::BadRecords;
 standard::Record rows[80]{};size_t count=0;
 if(standard::records(frame+15,n-15,rows,80,count)!=standard::Status::Ok)
   return Status::BadRecords;
 Reading next{};bool current=false,at_key=false,volume=false;
 for(size_t i=0;i<count;++i) {
   const auto&r=rows[i];
   if(!r.numeric || r.function || r.tariff || r.subunit ||
      r.vif_extension_count || r.modifier_count)continue;
   if(r.vif<=0x07 && r.storage<=1) {
     const double v=r.value*standard::scale10(static_cast<int>(r.vif)-6);
     if(!std::isfinite(v) || v<0 || v>10000000000.0)
       return Status::BadRecords;
     if(r.storage==0) {
       if(current)return Status::Duplicate;
       current=true;next.total_energy_consumption_kwh=v;
     } else {
       if(at_key)return Status::Duplicate;
       at_key=true;next.key_datetime_kwh=v;
     }
   } else if(r.vif>=0x10 && r.vif<=0x17 && r.storage==0) {
     double v=0;
     if(!standard::to_canonical(r.vif,r.value,v) ||
        !std::isfinite(v) || v<0 || v>100000000)
        return Status::BadRecords;
     if(volume)return Status::Duplicate;
     volume=true;next.has_total_m3=true;next.total_m3=v;
   }
 }
 if(!current || !at_key)return Status::Missing;
 // Billing reading cannot exceed latest cumulative reading.
 if(next.key_datetime_kwh>next.total_energy_consumption_kwh)
   return Status::BadRecords;
 out=next;return Status::Ok;
}
}} // namespace xmq_portable::qheat_55_us
