#pragma once
// Apator Metra E-ITN 40 APA/09 or 0D/08, CI=A0 clear manufacturer
// 54-byte profile from original wmbusmeters/drivers/src/apatoreitn40.xmq fixtures.
// Precisely 27 x uint16 LE from frame[11]. 0A/0E versions use AES
// and are NOT clear despite their detection signature: reject them.
// No generic OMS record parser: the payload uses proprietary words.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "native_publication_gate.h"
namespace xmq_portable { namespace apatoreitn40 {
enum class Status:uint8_t {Ok,Invalid,WrongModel,NotClear,BadRecords};
struct Reading {
 double current_hca{0},previous_hca{0},previous_2y_hca{0},
 previous_3y_hca{0},current_month_hca{0},previous_month_hca{0},
 temp_room_c{0},temp_radiator_c{0};
};
inline uint16_t word(const uint8_t *f,size_t i) {
 const size_t off=11u+2u*i;
 return static_cast<uint16_t>(f[off]|(static_cast<uint16_t>(f[off+1])<<8));
}
inline bool valid_date(uint16_t raw) {
 const unsigned day=raw&31,month=(raw>>5)&15,year=(raw>>9)&127;
 if(month<1||month>12||day<1||year>99)return false;
 const unsigned maxdays=(month==2?29:
     (month==4||month==6||month==9||month==11?30:31));
 return day<=maxdays;
}
inline Status decode(const uint8_t *f,size_t n,Reading &out) {
 out={};wmbus_pure::Header h{};
 if(wmbus_pure::parse_header(f,n,h)!=wmbus_pure::HeaderStatus::Ok)
   return Status::Invalid;
 if(h.control!=0x44 || h.manufacturer!=0x0601 ||
    (h.version!=0x09 && h.version!=0x0D) ||
    h.device_type!=0x08 || h.ci!=0xA0)return Status::WrongModel;
 if(wmbus_pure::publication::inspect(f,n,h.meter_id)!=
    wmbus_pure::publication::Decision::Allowed)return Status::NotClear;
 if(n!=65 || word(f,0)!=0 || word(f,26)!=0 ||
    !valid_date(word(f,1)) || !valid_date(word(f,24)) ||
    word(f,25)>6)
   return Status::BadRecords;
 Reading next{};
 next.current_month_hca=word(f,4);
 next.previous_month_hca=word(f,5);
 next.current_hca=word(f,20);
 next.previous_hca=word(f,21);
 next.previous_2y_hca=word(f,22);
 next.previous_3y_hca=word(f,23);
 next.temp_radiator_c=static_cast<int16_t>(word(f,2))/16.0;
 next.temp_room_c=static_cast<int16_t>(word(f,3))/16.0;
 if(!std::isfinite(next.temp_radiator_c) ||
    !std::isfinite(next.temp_room_c) ||
    next.temp_radiator_c < -50 || next.temp_radiator_c > 150 ||
    next.temp_room_c < -50 || next.temp_room_c > 150)
   return Status::BadRecords;
 out=next;return Status::Ok;
}
}} // namespace xmq_portable::apatoreitn40
