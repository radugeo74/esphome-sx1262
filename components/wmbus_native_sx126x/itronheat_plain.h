#pragma once
// Itron/Allmess UltraMaXX ITW/00/04 one genuine clear short-TPL
// source fixture. Source: wmbusmeters/drivers/src/itronheat.xmq
// GPL-3.0-or-later. NOKEY supplied-clear Mode5 is never RF plaintext.
// Bounded 65-byte layout; opaque manufacturer records are not decoded.
// One public source frame uses ITW maker bit15 set (0xA697),
// so accept only 0x2697 and that exact evidence-backed variant.
#include <cstddef>
#include <cstdint>
#include <cmath>
#include "native_publication_gate.h"
#include "standard_dif_vif.h"
namespace xmq_portable { namespace itronheat {
enum class Status:uint8_t {Ok,Invalid,WrongModel,NotClear,BadRecords};
struct Reading {double total_kwh{0},total_m3{0},last_year_kwh{0};};
inline bool energy(const uint8_t *f,size_t n,uint32_t storage,double &out) {
 standard::Record r[1]{};size_t count=0;
 if(standard::records(f,n,r,1,count)!=standard::Status::Ok ||
    count!=1 || !r[0].numeric || r[0].dif!=(storage==0?0x04:0x44) ||
    r[0].vif!=0x06 || r[0].storage!=storage || r[0].function ||
    r[0].tariff || r[0].subunit || r[0].vif_extension_count ||
    r[0].modifier_count)return false;
 out=r[0].value;
 return std::isfinite(out)&&out>=0&&out<10000000000.0;
}
inline bool volume(const uint8_t *f,double &out) {
 standard::Record r[1]{};size_t count=0;
 if(standard::records(f,6,r,1,count)!=standard::Status::Ok ||
    count!=1 || !r[0].numeric || r[0].dif!=0x04 ||
    r[0].vif!=0x13 || r[0].storage || r[0].function ||
    r[0].tariff || r[0].subunit || r[0].vif_extension_count ||
    r[0].modifier_count)return false;
 return standard::to_canonical(0x13,r[0].value,out) &&
        std::isfinite(out)&&out>=0&&out<100000000.0;
}
inline Status decode(const uint8_t *f,size_t n,Reading &out) {
 out={};wmbus_pure::Header h{};
 if(wmbus_pure::parse_header(f,n,h)!=wmbus_pure::HeaderStatus::Ok)
   return Status::Invalid;
 if(h.control!=0x44 ||
    (h.manufacturer!=0x2697 && h.manufacturer!=0xA697) ||
    h.version!=0x00 || h.device_type!=0x04 || h.ci!=0x7A)
   return Status::WrongModel;
 if(wmbus_pure::publication::inspect(f,n,h.meter_id)!=
    wmbus_pure::publication::Decision::Allowed)return Status::NotClear;
 if(n!=65 || f[12]!=0 || f[13]!=0 || f[14]!=0xA0 ||
    f[15]!=0x04 || f[16]!=0x06 || f[21]!=0x04 || f[22]!=0x13 ||
    f[27]!=0x06 || f[28]!=0x6D || f[35]!=0x44 || f[36]!=0x06 ||
    f[41]!=0x42 || f[42]!=0x6C || f[45]!=0x0D || f[46]!=0x7F)
   return Status::BadRecords;
 Reading candidate{};
 if(!energy(f+15,6,0,candidate.total_kwh) ||
    !volume(f+21,candidate.total_m3) ||
    !energy(f+35,6,1,candidate.last_year_kwh) ||
    candidate.last_year_kwh>candidate.total_kwh)
   return Status::BadRecords;
 out=candidate;return Status::Ok;
}
}} // namespace xmq_portable::itronheat
