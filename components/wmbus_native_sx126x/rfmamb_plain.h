#pragma once
// Bmeters RFM-AMB: strict, independently configured, actually clear TPL Mode-0.
// Upstream reference: wmbusmeters/drivers/src/rfmamb.xmq (GPL-3.0-or-later).
// Input MUST have full verified DLL CRC before this application parser.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "native_publication_gate.h"
#include "standard_dif_vif.h"
namespace xmq_portable {namespace rfmamb {
enum class Status:uint8_t {Ok,Invalid,WrongModel,Encrypted,Records,Duplicate,Missing};
struct Reading {
 double current_temperature_c{0},average_temperature_1h_c{0};
 double average_temperature_24h_c{0};
 double current_relative_humidity_rh{0},average_relative_humidity_1h_rh{0};
 double average_relative_humidity_24h_rh{0};
 uint8_t tpl_status{0};
};
inline Status decode(const uint8_t*frame,size_t length,Reading&out) {
 out={};wmbus_pure::Header header{};
 if(wmbus_pure::parse_header(frame,length,header)!=
    wmbus_pure::HeaderStatus::Ok)return Status::Invalid;
 if(header.control!=0x44 || header.manufacturer!=0x09B4 ||
    header.version!=0x10 || header.device_type!=0x1B || header.ci!=0x7A)
    return Status::WrongModel;
 if(wmbus_pure::publication::inspect(frame,length,header.meter_id)!=
    wmbus_pure::publication::Decision::Allowed)return Status::Encrypted;
 if(length<=15)return Status::Records;
 standard::Record records[32]{};size_t count=0;
 if(standard::records(frame+15,length-15,records,32,count)!=
    standard::Status::Ok)return Status::Records;
 Reading next{};unsigned found=0;next.tpl_status=frame[12];
 for(size_t i=0;i<count;++i) {
   const auto &r=records[i];
   if(!r.numeric || r.tariff || r.subunit || r.function ||
      r.modifier_count || r.storage>2)continue;
   int slot=-1;double value=0;
   if(r.vif_extension_count==0 && r.vif==0x65) {
     if(!standard::to_canonical(r.vif,r.value,value))return Status::Records;
     slot=int(r.storage);
   } else if(r.vif_extension_count==1 && r.full_vif==0x7B1A) {
     // VIF FB 1A: humidity in tenths of percent.
     value=r.value/10.0;slot=3+int(r.storage);
   }
   if(slot<0)continue;
   if((found & (1u<<slot))!=0)return Status::Duplicate;
   if(!std::isfinite(value) ||
      (slot<3 && (value< -100.0 || value>150.0)) ||
      (slot>=3 && (value<0 || value>100)))return Status::Records;
   found|=(1u<<slot);
   switch(slot){
     case 0:next.current_temperature_c=value;break;
     case 1:next.average_temperature_1h_c=value;break;
     case 2:next.average_temperature_24h_c=value;break;
     case 3:next.current_relative_humidity_rh=value;break;
     case 4:next.average_relative_humidity_1h_rh=value;break;
     case 5:next.average_relative_humidity_24h_rh=value;break;
   }
 }
 if(found!=0x3F)return Status::Missing;
 out=next;return Status::Ok;
}
}}