#pragma once
// Axioma Qualcosonic AXI/0B-0C/0D clear short-TPL standard-field subset.
// Source: wmbusmeters/drivers/src/qualcosonic.xmq GPL-3.0-or-later.
// No generic energy/cooling VIF modifier support, no AES, no false alarms.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "native_publication_gate.h"
#include "standard_dif_vif.h"
namespace xmq_portable { namespace qualcosonic {
enum class Status:uint8_t {Ok,Invalid,WrongModel,NotClear,BadRecords,Duplicate,Missing};
struct Reading {double total_m3{0},flow_temperature_c{0};
  double return_temperature_c{0},power_kw{0};};
inline Status decode(const uint8_t *frame,size_t length,Reading &out) {
 out={};wmbus_pure::Header h{};
 if(wmbus_pure::parse_header(frame,length,h)!=
    wmbus_pure::HeaderStatus::Ok)return Status::Invalid;
 if(h.control!=0x44 || h.manufacturer!=0x0709 ||
    (h.version!=0x0B && h.version!=0x0C) ||
    h.device_type!=0x0D || h.ci!=0x7A)return Status::WrongModel;
 if(wmbus_pure::publication::inspect(frame,length,h.meter_id)!=
    wmbus_pure::publication::Decision::Allowed)return Status::NotClear;
 // The three upstream public clear frames are 119/155-byte profiles
 // with status 0, no encryption and leading 04 6D measurement datetime.
 if((length!=119 && length!=155) || frame[12]!=0 ||
    frame[13]!=0 || frame[14]!=0 ||
    frame[15]!=0x04 || frame[16]!=0x6D)
   return Status::BadRecords;
 standard::Record rec[64]{};size_t count=0;
 if(standard::records(frame+15,length-15,rec,64,count)!=
    standard::Status::Ok)return Status::BadRecords;
 Reading next{};unsigned mask=0;
 for(size_t i=0;i<count;++i) {
   const auto &r=rec[i];
   if(!r.numeric || r.function || r.storage || r.tariff ||
      r.subunit || r.vif_extension_count || r.modifier_count)continue;
   unsigned slot=4;double value=0;
   if(r.vif==0x13)slot=0;
   else if(r.vif==0x59)slot=1;
   else if(r.vif==0x5D)slot=2;
   else if(r.vif==0x2B)slot=3;
   if(slot==4)continue;
   if(mask&(1u<<slot))return Status::Duplicate;
   if(slot==3) value=r.value*standard::scale10(-3);
   else if(!standard::to_canonical(r.vif,r.value,value))
     return Status::BadRecords;
   if(!std::isfinite(value) ||
      (slot==0 && (value<0 || value>100000000)) ||
      ((slot==1 || slot==2) && (value< -50 || value>180)) ||
      (slot==3 && (value< -100000 || value>100000)))
     return Status::BadRecords;
   mask|=1u<<slot;
   if(slot==0)next.total_m3=value;
   else if(slot==1)next.flow_temperature_c=value;
   else if(slot==2)next.return_temperature_c=value;
   else next.power_kw=value;
 }
 if(mask!=0x0F)return Status::Missing;
 out=next;return Status::Ok;
}
}} // namespace xmq_portable::qualcosonic
