#pragma once
// Techem Compact5: TWO source-original clear TCH/39/43, CI A2 56-byte
// radio profiles; upstream drivers/src/compact5.xmq GPL-3.0-or-later.
// The two CI A1 profiles remain unsupported; no TPL policy relaxation.
#include <cstddef>
#include <cstdint>
#include "native_publication_gate.h"
namespace xmq_portable { namespace compact5 {
enum class Status:uint8_t {Ok,Invalid,WrongModel,NotClear,BadRecords};
struct Reading {double total_kwh{0},current_kwh{0},previous_kwh{0};};
inline uint32_t u24(const uint8_t*p) {
 return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16);
}
inline Status decode(const uint8_t*f,size_t n,Reading &out) {
 out={};wmbus_pure::Header h{};
 if(wmbus_pure::parse_header(f,n,h)!=wmbus_pure::HeaderStatus::Ok)
   return Status::Invalid;
 if(h.control!=0x44 || h.manufacturer!=0x5068 ||
    h.version!=0x39 || h.device_type!=0x43 || h.ci!=0xA2)
   return Status::WrongModel;
 if(wmbus_pure::publication::inspect(f,n,h.meter_id)!=
    wmbus_pure::publication::Decision::Allowed)
   return Status::NotClear;
 // Original source IXML fields: three-byte header, u24 previous,
 // one-byte separator, u24 current. Rest is not decoded.
 if(n!=56 || f[11]!=0x10 ||
    !((f[12]==0x67 && f[13]==0x2C && f[17]==0x18) ||
      (f[12]==0x9F && f[13]==0x2F && f[17]==0x60)))
   return Status::BadRecords;
 const uint32_t prev=u24(f+14),curr=u24(f+18);
 if(prev>10000000u || curr>10000000u ||
    prev+curr>10000000u) return Status::BadRecords;
 out.previous_kwh=prev;out.current_kwh=curr;out.total_kwh=prev+curr;
 return Status::Ok;
}
}} // namespace xmq_portable::compact5
