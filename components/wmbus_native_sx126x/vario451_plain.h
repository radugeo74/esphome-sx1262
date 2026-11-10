#pragma once
// Techem Vario451: exact TCH/27/C3 CI A2 56-byte original radio profile;
// drivers/src/vario451.xmq GPL-3.0-or-later. No generic encryption bypass.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include "native_publication_gate.h"
#include "xmq_generated_vario451.h"
namespace xmq_portable { namespace vario451 {
enum class Status:uint8_t {Ok,Invalid,WrongModel,NotClear,BadRecords};
struct Reading {
 double total_kwh{0},current_kwh{0},previous_kwh{0};
 uint16_t prev_raw_counter{0},curr_raw_counter{0};
 char mfct_payload_hex[91]{}; // 45-byte source-verified CI-A2 payload
};
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
 const uint16_t prev=u16(f+11+xmq_generated_vario451::previous_payload_offset);
 const uint16_t curr=u16(f+11+xmq_generated_vario451::current_payload_offset);
 const double divisor=xmq_generated_vario451::kwh_divisor;
 Reading next{};
 next.prev_raw_counter=prev;next.curr_raw_counter=curr;
 next.previous_kwh=prev/divisor;next.current_kwh=curr/divisor;
 next.total_kwh=(static_cast<uint32_t>(prev)+curr)/divisor;
 // Preserve the entire manufacturer-specific payload for opt-in
 // diagnostics. Uninterpreted bytes are never presented as measurements.
 for(size_t i=0;i<45;++i) {
   const int written=std::snprintf(next.mfct_payload_hex+i*2,3,
       "%02X",static_cast<unsigned>(f[11+i]));
   if(written!=2)return Status::BadRecords;
 }
 out=next;
 return Status::Ok;
}
}} // namespace xmq_portable::vario451
