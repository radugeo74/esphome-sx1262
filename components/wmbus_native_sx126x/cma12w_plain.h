#pragma once
// Elvaco CMA12w native compatibility adapter.
// ALL source-specific record shapes, conversion instructions, XMQ
// model/config variants and status admission live in generated profile
// descriptors. The shared, bounded C++ interpreter decodes them.
// Original upstream source: cma12w.xmq (GPL-3.0-or-later).
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "xmq_full_profile_runtime.h"
namespace xmq_portable { namespace cma12w {
enum class Status : uint8_t {
 Ok, InvalidFrame, WrongModel, EncryptedOrUnknown, InvalidStatus,
 BadRecords, Duplicate, Missing, SoftwareVersion
};
struct Reading {
 double current_temperature_c{0};
 double average_temperature_1h_c{0};
 uint16_t battery_bits{0};
 uint8_t tpl_status{0};
 char software_version[8]{};
};
inline const char* status_text(uint8_t tpl) {
 const auto &p=xmq_compiled_profiles::cma12w::profile;
 for(size_t i=0;i<p.state_count;++i)
   if(p.states[i].tpl==tpl && p.states[i].error==0)
     return p.states[i].text;
 return nullptr;
}
inline Status decode(const uint8_t *frame,size_t length,Reading &out) {
 out={};
 full_xmq::Result decoded{};
 const auto result=full_xmq::decode(frame,length,
     xmq_compiled_profiles::cma12w::profile,decoded);
 if(result!=full_xmq::Status::Ok) {
   if(result==full_xmq::Status::WrongModel) return Status::WrongModel;
   if(result==full_xmq::Status::EncryptedOrUnknown)
     return Status::EncryptedOrUnknown;
   if(result==full_xmq::Status::UnsupportedStatus)
     return Status::InvalidStatus;
   if(result==full_xmq::Status::Duplicate) return Status::Duplicate;
   if(result==full_xmq::Status::Missing) return Status::Missing;
   return Status::BadRecords;
 }
 Reading next{};
 next.current_temperature_c=decoded.numeric[0];
 next.average_temperature_1h_c=decoded.numeric[1];
 next.battery_bits=decoded.raw16[2];
 next.tpl_status=decoded.tpl_status;
 std::snprintf(next.software_version,sizeof(next.software_version),"%s",
               decoded.formatted[3]);
 out=next;
 return Status::Ok;
}
}} // namespace xmq_portable::cma12w
