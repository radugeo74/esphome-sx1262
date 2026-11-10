#pragma once
// Techem Vario451: exact TCH/27/C3 CI A2 56-byte original radio profile;
// drivers/src/vario451.xmq GPL-3.0-or-later. No generic encryption bypass.
#include <cstddef>
#include <cstdint>
#include "native_publication_gate.h"
#include "xmq_generated_vario451.h"
namespace xmq_portable { namespace vario451 {
enum class Status:uint8_t {Ok,Invalid,WrongModel,NotClear,BadRecords};
struct Reading {double total_kwh{0},current_kwh{0},previous_kwh{0};};
inline uint16_t u16(const uint8_t*p) {
 return static_cast<uint16_t>(p[0]|(static_cast<uint16_t>(p[1])<<8));
}
inline Status decode(const uint8_t*f,size_t n,Reading &out) {
 out={};wmbus_pure::Header h{};
 if(wmbus_pure::parse_header(f,n,h)!=wmbus_pure::HeaderStatus::Ok)
   return Status::Invalid;
 if(h.control!=0x44 ||
    h.manufacturer!=xmq_generated_vario451::manufacturer ||
    h.version!=xmq_generated_vario451::version ||
    h.device_type!=xmq_generated_vario451::device_type ||
    h.ci!=xmq_generated_vario451::ci)
   return Status::WrongModel;
 if(wmbus_pure::publication::inspect(f,n,h.meter_id)!=
    wmbus_pure::publication::Decision::Allowed)
   return Status::NotClear;
 // Source layout: header[3], previous[2], spacer[2], current[2].
 // Source mGJ values /3.6 yield kWh. Opaque vendor tail unparsed.
 if(n!=56 || f[11]!=0x12 || f[12]!=0x9F || f[13]!=0x25 ||
    f[16]!=0x00 || f[17]!=0xA8 || f[20]!=0x00 ||
    f[21]!=0x00 || f[22]!=0x82)
   return Status::BadRecords;
 // Position and scaling are compiled from the original XMQ IXML.
 // The strict frame/CI/ID/security admission above is not relaxed.
 const uint32_t prev=u16(f+11+xmq_generated_vario451::previous_payload_offset);
 const uint32_t curr=u16(f+11+xmq_generated_vario451::current_payload_offset);
 const double divisor=xmq_generated_vario451::kwh_divisor;
 out.previous_kwh=prev/divisor;out.current_kwh=curr/divisor;
 out.total_kwh=(prev+curr)/divisor;
 return Status::Ok;
}
}} // namespace xmq_portable::vario451
