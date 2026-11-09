#pragma once
// Maddalena MAD/01/07 strict public Mode-0 79-byte water application.
// Source: wmbusmeters/drivers/src/maddalena.xmq GPL-3.0-or-later.
// Upstream detect.mvt also lists MAD/01/06 but no original fixture
// confirms its application layout: this narrow decoder rejects it.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "standard_dif_vif.h"
#include "native_publication_gate.h"
namespace xmq_portable { namespace maddalena {
enum class Status:uint8_t {Ok,Invalid,WrongModel,NotClear,BadRecords,Missing,Duplicate};
struct Reading {double total_m3{0},target_m3{0};};
inline Status decode(const uint8_t *frame,size_t n,Reading &out) {
 out={};wmbus_pure::Header h{};
 if(wmbus_pure::parse_header(frame,n,h)!=wmbus_pure::HeaderStatus::Ok)
   return Status::Invalid;
 if(h.control!=0x44 || h.manufacturer!=0x3424 ||
    h.version!=0x01 || h.device_type!=0x07 || h.ci!=0x7A)
   return Status::WrongModel;
 if(wmbus_pure::publication::inspect(frame,n,h.meter_id)!=
    wmbus_pure::publication::Decision::Allowed)return Status::NotClear;
 // Original sample: two fill bytes, 04 13 total, 04 6D datetime,
 // storage-1 44 13 billing, 42 6C date, storage-2 history follows.
 if(n!=79 || frame[12]!=0 || frame[13]!=0 || frame[14]!=0x20 ||
    frame[15]!=0x2F || frame[16]!=0x2F ||
    frame[17]!=0x04 || frame[18]!=0x13 ||
    frame[23]!=0x04 || frame[24]!=0x6D ||
    frame[44]!=0x44 || frame[45]!=0x13 ||
    frame[50]!=0x42 || frame[51]!=0x6C)
   return Status::BadRecords;
 standard::Record rows[32]{};size_t count=0;
 if(standard::records(frame+15,n-15,rows,32,count)!=standard::Status::Ok)
   return Status::BadRecords;
 Reading next{};bool total=false,target=false;
 for(size_t i=0;i<count;++i) {
   const auto&r=rows[i];
   if(!r.numeric || r.function || r.tariff || r.subunit ||
      r.vif_extension_count || r.modifier_count ||
      r.vif!=0x13 || r.storage>1)continue;
   double val=0;
   if(!standard::to_canonical(0x13,r.value,val) ||
      !std::isfinite(val) || val<0 || val>100000000)
      return Status::BadRecords;
   if(r.storage==0) {
     if(total)return Status::Duplicate;
     total=true;next.total_m3=val;
   } else {
     if(target)return Status::Duplicate;
     target=true;next.target_m3=val;
   }
 }
 if(!total || !target)return Status::Missing;
 if(next.target_m3>next.total_m3)return Status::BadRecords;
 out=next;return Status::Ok;
}
}} // namespace xmq_portable::maddalena
