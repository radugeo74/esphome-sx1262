#pragma once
// BFW 240 radio BFW/02/08: strict fixed, native manufacturer clear subset.
// Upstream wmbusmeters/drivers/src/bfw240radio.xmq GPL-3.0-or-later.
// This is NOT a generic DIF/VIF payload; original IXML documents the offset
// and two-byte, high-byte-first HCA counters.
#include <cstddef>
#include <cstdint>
#include "native_publication_gate.h"
namespace xmq_portable { namespace bfw240radio {
enum class Status:uint8_t {Ok,Invalid,WrongModel,NotClear,BadPayload};
struct Reading {double current_hca{0},prev_hca{0};};
inline bool bcd_date(uint8_t x,uint8_t limit) {
 const uint8_t lo=x&0x0F,hi=x>>4;
 return lo<=9 && hi<=9 && hi*10+lo>=1 && hi*10+lo<=limit;
}
inline Status decode(const uint8_t *frame,size_t length,Reading &out) {
 out={};wmbus_pure::Header h{};
 if(wmbus_pure::parse_header(frame,length,h)!=wmbus_pure::HeaderStatus::Ok)
   return Status::Invalid;
 if(h.control!=0x44 || h.manufacturer!=0x08D7 ||
    h.version!=0x02 || h.device_type!=0x08 || h.ci!=0x7A)
   return Status::WrongModel;
 if(wmbus_pure::publication::inspect(frame,length,h.meter_id)!=
    wmbus_pure::publication::Decision::Allowed)return Status::NotClear;
 // The six original samples have exactly this opaque application layout.
 // Bytes 15,16 are padding; 17,18 vendor prefix; 19..22 counters;
 // final three bytes: BCD day/month/year.
 if(length!=55 || frame[12]!=0xC0 || frame[13]!=0 || frame[14]!=0 ||
    frame[15]!=0x2F || frame[16]!=0x2F || frame[18]!=0x1F ||
    !bcd_date(frame[52],31) || !bcd_date(frame[53],12) ||
    (frame[54]>>4)>9 || (frame[54]&15)>9)
   return Status::BadPayload;
 Reading next{};
 next.prev_hca=static_cast<double>((uint16_t(frame[19])<<8)|frame[20]);
 next.current_hca=static_cast<double>((uint16_t(frame[21])<<8)|frame[22]);
 out=next;return Status::Ok;
}
}} // namespace xmq_portable::bfw240radio
