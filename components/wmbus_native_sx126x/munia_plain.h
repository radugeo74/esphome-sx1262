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
#include "xmq_full_profile_runtime.h"
namespace xmq_portable { namespace munia {
enum class Status : uint8_t {Ok,Invalid,WrongModel,NotClear,BadRecords,Missing,Duplicate};
struct Reading {
 double current_temperature_c{0},current_relative_humidity_rh{0};
 uint8_t tpl_status{0};
 uint16_t error_flags{0};
};
inline const char *status_text(uint8_t tpl,uint16_t flags) {
 const auto &p=xmq_compiled_profiles::munia::profile;
 for(size_t i=0;i<p.state_count;++i)
   if(p.states[i].tpl==tpl && p.states[i].error==flags)
     return p.states[i].text;
 return nullptr;
}
inline Status decode(const uint8_t *frame,size_t length,Reading &out) {
 out={};
 // Keep the independent configured-meter publication policy. All
 // application parsing and field conversions are in the common full-XMQ
 // decoder, driven by source-generated model, record and status tables.
 wmbus_pure::Header h{};
 if(wmbus_pure::parse_header(frame,length,h)!=wmbus_pure::HeaderStatus::Ok)
   return Status::Invalid;
 if(wmbus_pure::publication::inspect(frame,length,h.meter_id)!=
    wmbus_pure::publication::Decision::Allowed)return Status::NotClear;
 full_xmq::Result decoded{};
 const auto status=full_xmq::decode(
     frame,length,xmq_compiled_profiles::munia::profile,decoded);
 if(status!=full_xmq::Status::Ok) {
   if(status==full_xmq::Status::WrongModel)return Status::WrongModel;
   if(status==full_xmq::Status::EncryptedOrUnknown)return Status::NotClear;
   if(status==full_xmq::Status::Duplicate)return Status::Duplicate;
   if(status==full_xmq::Status::Missing)return Status::Missing;
   return Status::BadRecords;
 }
 Reading result{};
 result.current_temperature_c=decoded.numeric[0];
 result.current_relative_humidity_rh=decoded.numeric[1];
 result.tpl_status=decoded.tpl_status;
 result.error_flags=decoded.raw16[2];
 if(!std::isfinite(result.current_temperature_c) ||
    !std::isfinite(result.current_relative_humidity_rh) ||
    result.current_temperature_c<-60 || result.current_temperature_c>120 ||
    result.current_relative_humidity_rh<0 ||
    result.current_relative_humidity_rh>100)return Status::BadRecords;
 out=result;
 return Status::Ok;
}
}} // namespace xmq_portable::munia
