#pragma once
// Itron/Allmess water ITW: exact clear short-TPL 57-byte profile.
// Source: wmbusmeters/drivers/src/itron.xmq GPL-3.0-or-later.
// Caller provides CRC-checked, CRC-stripped DLL frame and matching meter ID.
// No long CI72, Mode5, or proprietary flag/identity inference.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "native_publication_gate.h"
#include "standard_dif_vif.h"
namespace xmq_portable { namespace itron {
enum class Status:uint8_t {Ok,Invalid,WrongModel,NotClear,BadRecords};
struct Reading {double total_m3{0},target_m3{0};};
inline bool volume(const uint8_t *f,size_t n,unsigned storage,double &v) {
 standard::Record rec[1]{};size_t count=0;
 if(standard::records(f,n,rec,1,count)!=standard::Status::Ok ||
    count!=1 || !rec[0].numeric || rec[0].vif!=0x13 ||
    rec[0].vif_extension_count || rec[0].modifier_count ||
    rec[0].storage!=storage || rec[0].function ||
    rec[0].tariff || rec[0].subunit)return false;
 return standard::to_canonical(0x13,rec[0].value,v) &&
        std::isfinite(v) && v>=0 && v<=100000000;
}
inline Status decode(const uint8_t *f,size_t n,Reading &out) {
 out={};wmbus_pure::Header h{};
 if(wmbus_pure::parse_header(f,n,h)!=wmbus_pure::HeaderStatus::Ok)
   return Status::Invalid;
 if(h.control!=0x44 || h.manufacturer!=0x2697 ||
    (h.version!=0x00 && h.version!=0x03 && h.version!=0x33) ||
    h.device_type!=0x07 || h.ci!=0x7A)
   return Status::WrongModel;
 if(wmbus_pure::publication::inspect(f,n,h.meter_id)!=
    wmbus_pure::publication::Decision::Allowed)return Status::NotClear;
 // The two public clear samples have status=0, CF=A000 (Mode0, N=0),
 // 04 13 total, 06 6D datetime, 44 13 target, 42 6C billing date,
 // followed by 04 7F / 02 7F proprietary flags.
 if(n!=57 || f[12]!=0 || f[13]!=0 || f[14]!=0xA0 ||
    f[15]!=0x04 || f[16]!=0x13 ||
    f[21]!=0x06 || f[22]!=0x6D ||
    f[29]!=0x44 || f[30]!=0x13 ||
    f[35]!=0x42 || f[36]!=0x6C ||
    f[39]!=0x04 || f[40]!=0x7F ||
    f[45]!=0x02 || f[46]!=0x7F)
   return Status::BadRecords;
 // The date record must be a real previous-period date.
 standard::Calendar date{};
 const uint32_t raw_date=uint32_t(f[37])|(uint32_t(f[38])<<8);
 if(!standard::decode_calendar(raw_date,false,date))
   return Status::BadRecords;
 Reading next{};
 if(!volume(f+15,6,0,next.total_m3) ||
    !volume(f+29,6,1,next.target_m3) ||
    next.target_m3>next.total_m3)return Status::BadRecords;
 out=next;return Status::Ok;
}
}} // namespace xmq_portable::itron
