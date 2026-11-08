#pragma once
// Engelmann Sensostar EFE/00/04: strict clear TPL Mode-0 energy/volume subset.
// Wired M-Bus, Mode-5 supplied-clear XMQ and unknown models are rejected.
// Upstream reference: wmbusmeters/drivers/src/sensostar.xmq GPL-3.0-or-later.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "native_publication_gate.h"
#include "standard_dif_vif.h"
namespace xmq_portable {namespace sensostar {
enum class Status:uint8_t {Ok,Invalid,WrongModel,Encrypted,Records,Duplicate,Missing};
struct Reading {
 double total_kwh{0},total_water_m3{0},target_kwh{0};
 bool has_target_kwh{false};
};
inline Status decode(const uint8_t*frame,size_t length,Reading&out) {
 out={};wmbus_pure::Header header{};
 if(wmbus_pure::parse_header(frame,length,header)!=
    wmbus_pure::HeaderStatus::Ok)return Status::Invalid;
 if(header.control!=0x44 || header.manufacturer!=0x14C5 ||
    header.version!=0x00 || header.device_type!=0x04 || header.ci!=0x7A)
    return Status::WrongModel;
 if(wmbus_pure::publication::inspect(frame,length,header.meter_id)!=
    wmbus_pure::publication::Decision::Allowed)return Status::Encrypted;
 if(length<=15 || frame[12]!=0)return Status::Records;
 standard::Record records[64]{};size_t count=0;
 if(standard::records(frame+15,length-15,records,64,count)!=
    standard::Status::Ok)return Status::Records;
 Reading next{};unsigned found=0;
 for(size_t i=0;i<count;++i) {
   const auto &r=records[i];
   if(!r.numeric || r.vif_extension_count || r.modifier_count ||
      r.tariff || r.subunit || r.function || r.storage>1)continue;
   unsigned slot=3;double value=0;
   if(r.vif==0x06) {
     // EN13757 VIF 06 is kWh, directly; storage=1 is latest target.
     value=r.value;
     slot=(r.storage==0?0u:2u);
   } else if(r.vif==0x13 && r.storage==0) {
     // EN13757 VIF 13: litres to m^3.
     if(!standard::to_canonical(r.vif,r.value,value))return Status::Records;
     slot=1;
   }
   if(slot>2)continue;
   if((found & (1u<<slot))!=0)return Status::Duplicate;
   if(!std::isfinite(value) || value<0)return Status::Records;
   found|=1u<<slot;
   if(slot==0)next.total_kwh=value;
   else if(slot==1)next.total_water_m3=value;
   else{next.target_kwh=value;next.has_target_kwh=true;}
 }
 if((found&0x03)!=0x03)return Status::Missing;
 out=next;return Status::Ok;
}
}}