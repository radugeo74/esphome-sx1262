#pragma once
// Engelmann HCA E2 EFE/31/08. Strict configured-only Mode-0 TPL subset.
// Upstream source: wmbusmeters/drivers/src/hcae2.xmq GPL-3.0-or-later.
// Only historical fixture-proven T1 frame profiles; not an RF test.
#include <cstddef>
#include <cstdint>
#include "standard_dif_vif.h"
#include "native_publication_gate.h"
namespace xmq_portable { namespace hcae2 {
enum class Status:uint8_t {Ok,Invalid,WrongModel,NotClear,BadRecords,Duplicate,Missing};
struct Reading {
 double consumption_hca{0},target_hca{0};
 uint8_t tpl_status{0},manufacturer_flags{0};
};
inline Status decode(const uint8_t *frame,size_t length,Reading &out) {
 out={};
 wmbus_pure::Header h{};
 if(wmbus_pure::parse_header(frame,length,h)!=wmbus_pure::HeaderStatus::Ok)
   return Status::Invalid;
 if((h.control!=0x44 && h.control!=0x46) ||
    h.manufacturer!=0x14C5 || h.version!=0x31 ||
    h.device_type!=0x08 || h.ci!=0x7A)return Status::WrongModel;
 if(wmbus_pure::publication::inspect(frame,length,h.meter_id)!=
    wmbus_pure::publication::Decision::Allowed)return Status::NotClear;
 // Original T1 Short/Long and ActiveLong profiles. The upstream C1
 // examples carry additional CRC bytes and are not treated as stripped RF.
 if((length!=46 && length!=202 && length!=207) ||
     frame[15]!=0x2F || frame[16]!=0x2F)return Status::BadRecords;
 standard::Record records[64]{};size_t count=0;
 if(standard::records(frame+15,length-15,records,64,count)!=
    standard::Status::Ok)return Status::BadRecords;
 Reading next{};next.tpl_status=frame[12];
 bool current=false,target=false,flags=false;
 for(size_t i=0;i<count;++i) {
   const auto &r=records[i];
   if(!r.numeric || r.modifier_count || r.tariff || r.subunit ||
      r.function)continue;
   if(r.vif_extension_count==0 && r.vif==0x6E &&
      (r.storage==0 || r.storage==1)) {
     if(r.value<0 || r.value>10000000)return Status::BadRecords;
     if(r.storage==0) {
       if(current)return Status::Duplicate;
       current=true;next.consumption_hca=r.value;
     } else {
       if(target)return Status::Duplicate;
       target=true;next.target_hca=r.value;
     }
   } else if(r.dif==0x31 && r.vif==0x7F &&
             r.vif_extension_count==0 && r.storage==0) {
     if(flags || r.value<0 || r.value>255)return Status::BadRecords;
     flags=true;next.manufacturer_flags=static_cast<uint8_t>(r.value);
   }
 }
 if(!current || !target || !flags)return Status::Missing;
 out=next;return Status::Ok;
}
inline const char *known_status(const Reading &r) {
 // Exactly observed upstream status patterns; unknown states are omitted.
 if(r.tpl_status==0x00 && r.manufacturer_flags==0)return "OK";
 if(r.tpl_status==0x0B && r.manufacturer_flags==0x1A)
   return "ALARM OPEN_C PERMANENT_ERROR SENSOR_BREAK UNEXPECTED_RST";
 return nullptr;
}
}} // namespace xmq_portable::hcae2
