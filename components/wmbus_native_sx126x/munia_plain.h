#pragma once
// WEP Munia room sensor WEP/02-04/1B: two exact public clear Mode0 profiles.
// Source: wmbusmeters/drivers/src/munia.xmq GPL-3.0-or-later.
// No undocumented humidity scale, status or RF key assumptions.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "native_publication_gate.h"
#include "standard_dif_vif.h"
#include "xmq_generated_clear_profiles.h"
namespace xmq_portable { namespace munia {
enum class Status : uint8_t {Ok,Invalid,WrongModel,NotClear,BadRecords,Missing,Duplicate};
struct Reading {
 double current_temperature_c{0},current_relative_humidity_rh{0};
 uint8_t tpl_status{0};
 uint16_t error_flags{0};
};
inline Status decode(const uint8_t *frame,size_t length,Reading &out) {
 out={};wmbus_pure::Header h{};
 if(wmbus_pure::parse_header(frame,length,h)!=wmbus_pure::HeaderStatus::Ok)
   return Status::Invalid;
 if(h.control!=0x44 || h.ci!=0x7A ||
    !xmq_generated_clear::match_model(h.manufacturer,h.version,h.device_type,
      xmq_generated_clear::munia::models,2))return Status::WrongModel;
 if(wmbus_pure::publication::inspect(frame,length,h.meter_id)!=
    wmbus_pure::publication::Decision::Allowed)return Status::NotClear;
 // Complete 47-byte profile:
 // 2F2F + 0A66(2 BCD temp) + 0AFB1A(2 BCD humidity)
 // + 02FD971D(2 zero flags) + terminal 2F padding.
 if(length!=47 || frame[12]!=0 || frame[13]!=0 || frame[14]!=0 ||
    frame[15]!=0x2F || frame[16]!=0x2F ||
    !xmq_generated_clear::match_rule(frame+17,length-17,
      xmq_generated_clear::munia::rules[0]) ||
    !xmq_generated_clear::match_rule(frame+21,length-21,
      xmq_generated_clear::munia::rules[1]) ||
    !xmq_generated_clear::match_rule(frame+26,length-26,
      xmq_generated_clear::munia::rules[2]) ||
    frame[30]!=0 || frame[31]!=0)
    return Status::BadRecords;
 for(size_t i=32;i<length;++i)if(frame[i]!=0x2F)return Status::BadRecords;
 standard::Record rec[4]{};size_t count=0;
 if(standard::records(frame+17,15,rec,4,count)!=standard::Status::Ok)
   return Status::BadRecords;
 Reading next{};bool temp=false,humidity=false;
 next.tpl_status=frame[12];
 next.error_flags=static_cast<uint16_t>(frame[30]) |
     (static_cast<uint16_t>(frame[31]) << 8);
 for(size_t i=0;i<count;++i) {
    const auto &r=rec[i];
    if(!r.numeric || r.storage || r.function || r.tariff ||
       r.subunit)continue;
    if(r.dif==0x0A && r.vif==0x66 && !r.vif_extension_count &&
       !r.modifier_count) {
      if(temp)return Status::Duplicate;
      if(!standard::to_canonical(r.vif,r.value,next.current_temperature_c))
        return Status::BadRecords;
      temp=true;
    } else if(r.dif==0x0A && r.full_vif==0x7B1A &&
              r.vif_extension_count==1 && !r.modifier_count) {
      if(humidity)return Status::Duplicate;
      next.current_relative_humidity_rh=r.value/10.0;
      humidity=true;
    }
 }
 if(!temp || !humidity)return Status::Missing;
 if(!std::isfinite(next.current_temperature_c) ||
    !std::isfinite(next.current_relative_humidity_rh) ||
    next.current_temperature_c<-60 || next.current_temperature_c>120 ||
    next.current_relative_humidity_rh<0 ||
    next.current_relative_humidity_rh>100)return Status::BadRecords;
 out=next;return Status::Ok;
}
}} // namespace xmq_portable::munia
