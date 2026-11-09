#pragma once
// Apator E-ITN heat-cost allocator, A0 manufacturer application and
// B6 length-bounded nested A0 variant, only three upstream radio fixtures.
// wmbusmeters/drivers/src/apatoreitn.xmq, GPL-3.0-or-later.
// Unlike the E-ITN40 this is NOT 27-word CI A0: 15-byte proprietary
// payload with season byte, previous/current HCA and Q8.8 temperatures.
// B6 is admitted ONLY for source-proven 0A/0E public profiles, not
// through the general publication/security policy.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "native_publication_gate.h"
namespace xmq_portable { namespace apatoreitn {
enum class Status:uint8_t {Ok,Invalid,WrongModel,NotClear,BadRecords};
struct Reading {
 double current_hca{0},previous_hca{0},
 temp_room_avg_c{0},temp_room_prev_avg_c{0};
 bool seal_broken{false};
};
inline uint16_t u16(const uint8_t *f,size_t off) {
 return static_cast<uint16_t>(f[off]|
                 (static_cast<uint16_t>(f[off+1])<<8));
}
inline bool valid_date(uint16_t x,bool allow_empty=false) {
 if(x==0)return allow_empty;
 const unsigned day=x&31u,month=(x>>5)&15u,year=(x>>9)&31u;
 if(month<1||month>12||day<1)return false;
 const unsigned feb=(year%4u==0u)?29u:28u;
 const unsigned limit=month==2?feb:(
   month==4||month==6||month==9||month==11?30u:31u);
 return day<=limit;
}
inline Status decode(const uint8_t*f,size_t n,Reading &out) {
 out={};wmbus_pure::Header h{};
 if(wmbus_pure::parse_header(f,n,h)!=wmbus_pure::HeaderStatus::Ok)
   return Status::Invalid;
 if(h.control!=0x44 || h.version!=0x04 ||
    h.device_type!=0x08 || !((h.ci==0xA0 && h.manufacturer==0x8601) ||
      (h.ci==0xB6 && h.manufacturer==0x8614)))
   return Status::WrongModel;
 size_t p=0;
 if(h.ci==0xA0) {
   if(n!=26 || wmbus_pure::publication::inspect(f,n,h.meter_id)!=
                   wmbus_pure::publication::Decision::Allowed)
     return Status::NotClear;
   p=11;
 } else {
   // B6 includes a proprietary count-prefixed radio envelope;
   // only the two 0A/0E original shapes are authorized. The
   // 0xFFFF / nested APA marker is source-verified: it does NOT
   // imply a standardized TPL, Mode0, or AES decryption.
   if(n==38 && f[11]==0x0A)p=23;
   else if(n==42 && f[11]==0x0E)p=27;
   else return Status::NotClear;
   if(f[12]!=0xFF || f[13]!=0xFF ||
      f[16]!=0x01 || f[17]!=0x86 || f[p-1]!=0xA0)
     return Status::BadRecords;
 }
 if(p+15u!=n || f[p]!=0xA1 || f[p+1]!=0 ||
    f[p+2]!=0)
   return Status::BadRecords;
 const uint16_t seal=u16(f,p+5),current_date=u16(f,p+9);
 // Underlying Apator date has year modulo 32, not standard CP16.
 if(!valid_date(current_date) || !valid_date(seal,true))
   return Status::BadRecords;
 Reading r{};
 r.previous_hca=u16(f,p+3);
 r.current_hca=u16(f,p+7);
 r.temp_room_prev_avg_c=u16(f,p+11)/256.0;
 r.temp_room_avg_c=u16(f,p+13)/256.0;
 r.seal_broken=seal!=0;
 if(!std::isfinite(r.temp_room_avg_c) ||
    !std::isfinite(r.temp_room_prev_avg_c) ||
    r.temp_room_avg_c<-40 || r.temp_room_avg_c>90 ||
    r.temp_room_prev_avg_c<-40 || r.temp_room_prev_avg_c>90)
   return Status::BadRecords;
 out=r;return Status::Ok;
}
}} // namespace xmq_portable::apatoreitn
