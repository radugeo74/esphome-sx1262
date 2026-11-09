#pragma once
// Ei Electronics Ei6500 EIE/0C/1A, six original public clear Mode0
// 89-byte profiles (wmbusmeters/drivers/src/ei6500.xmq).
// This is DIAGNOSTIC telemetry, NOT a smoke/fire safety signal.
// In particular status=OK in XMQ does not prove no smoke alarm: use
// only source-proven historic counters and dust level (0..31).
// Never publish a current smoke alarm state or safety reassurance.
#include <cstdint>
#include <cstddef>
#include "native_publication_gate.h"
namespace xmq_portable { namespace ei6500 {
enum class Status:uint8_t {Ok,Invalid,WrongModel,NotClear,BadRecords};
struct Reading {double alarm_counter{0},removed_counter{0},
 test_button_counter{0},dust_level_counter{0};};
inline Status decode(const uint8_t *f,size_t n,Reading &out) {
 out={};wmbus_pure::Header h{};
 if(wmbus_pure::parse_header(f,n,h)!=wmbus_pure::HeaderStatus::Ok)
   return Status::Invalid;
 if(h.control!=0x44 || h.manufacturer!=0x1525 ||
    h.version!=0x0C || h.device_type!=0x1A || h.ci!=0x7A)
   return Status::WrongModel;
 if(wmbus_pure::publication::inspect(f,n,h.meter_id)!=
    wmbus_pure::publication::Decision::Allowed)return Status::NotClear;
 // Exact original fixed record keys; no unknown layouts admitted.
 if(n!=89 || f[12]!=0 || f[13]!=0 || f[14]!=0 ||
    f[15]!=0x0B || f[16]!=0xFD || f[17]!=0x0F ||
    f[21]!=0x04 || f[22]!=0x6D ||
    f[27]!=0x02 || f[28]!=0xFD || f[29]!=0x17 ||
    f[30]!=0 || f[31]!=0 ||
    f[32]!=0x82 || f[33]!=0x20 || f[34]!=0x6C ||
    f[37]!=0x42 || f[38]!=0x6C ||
    f[41]!=0x84 || f[42]!=0x40 || f[43]!=0xFF || f[44]!=0x2C ||
    f[46]!=0x0F || f[47]!=0x11 || f[48]!=0 ||
    f[49]!=0x82 || f[50]!=0x50 || f[51]!=0xFD || f[52]!=0x61 ||
    f[55]!=0x82 || f[56]!=0x50 || f[57]!=0x6C ||
    f[60]!=0x82 || f[61]!=0x60 || f[62]!=0xFD || f[63]!=0x61 ||
    f[66]!=0x83 || f[67]!=0x60 || f[68]!=0xFD || f[69]!=0x31 ||
    f[73]!=0x82 || f[74]!=0x60 || f[75]!=0x6C ||
    f[78]!=0x82 || f[79]!=0x70 || f[80]!=0xFD || f[81]!=0x61 ||
    f[84]!=0x82 || f[85]!=0x70 || f[86]!=0x6C)
   return Status::BadRecords;
 // IndexRaw 8440FF2C bit 6 describes removal; low 5 bits describe dust.
 // None of these fields represents a live smoke/fire alarm state.
 out.dust_level_counter=static_cast<double>(f[45]&0x1Fu);
 out.alarm_counter=static_cast<double>(f[53]|(f[54]<<8));
 out.removed_counter=static_cast<double>(f[64]|(f[65]<<8));
 out.test_button_counter=static_cast<double>(f[82]|(f[83]<<8));
 return Status::Ok;
}
}} // namespace xmq_portable::ei6500
