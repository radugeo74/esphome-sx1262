#pragma once
// Techem Vario451: exact TCH/27/C3 CI A2 56-byte original radio profile;
// drivers/src/vario451.xmq GPL-3.0-or-later. No generic encryption bypass.
#include <cstddef>
#include <cstdint>
#include "native_publication_gate.h"
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
 if(h.control!=0x44 || h.manufacturer!=0x5068 ||
    h.version!=0x27 || h.device_type!=0xC3 || h.ci!=0xA2)
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
 const uint32_t prev=u16(f+14),curr=u16(f+18);
 out.previous_kwh=prev/3.6;out.current_kwh=curr/3.6;
 out.total_kwh=(prev+curr)/3.6;
 return Status::Ok;
}
}} // namespace xmq_portable::vario451
