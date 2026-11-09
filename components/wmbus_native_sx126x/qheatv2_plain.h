#pragma once
// Qundis/LSE QHeat V2: strictly verified short-TPL CI7A Mode0
// LSE/23/04 47-byte original fixture. Upstream qheatv2.xmq,
// GPL-3.0-or-later. Other 9 upstream fixtures use CI72 or CI78,
// including encrypted proprietary WalkBy; no generic plaintext fallback.
// Public fixture's M-field is 0xB265 (LSE bit15 set), NOT generic
// masking of all manufacturer ids. AUTO has a narrowly scoped alias.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "native_publication_gate.h"
#include "standard_dif_vif.h"
namespace xmq_portable { namespace qheatv2 {
enum class Status:uint8_t {Ok,Invalid,WrongModel,NotClear,BadRecords};
struct Reading {double total_kwh{0},target_year_kwh{0};};
inline bool energy(const uint8_t *f,size_t n,uint32_t storage,double &out) {
 standard::Record rec[1]{};size_t count=0;
 if(standard::records(f,n,rec,1,count)!=standard::Status::Ok ||
    count!=1 || !rec[0].numeric ||
    rec[0].dif!=(storage==0?0x0C:0x4C) ||
    rec[0].vif!=0x05 || rec[0].storage!=storage ||
    rec[0].tariff || rec[0].subunit || rec[0].function ||
    rec[0].modifier_count || rec[0].vif_extension_count)return false;
 out=rec[0].value*standard::scale10(-1); // 0x05: 100 Wh = 0.1 kWh
 return std::isfinite(out) && out>=0 && out<10000000000.;
}
inline Status decode(const uint8_t *f,size_t n,Reading &out) {
 out={};wmbus_pure::Header h{};
 if(wmbus_pure::parse_header(f,n,h)!=wmbus_pure::HeaderStatus::Ok)
   return Status::Invalid;
 if(h.control!=0x44 || h.manufacturer!=0xB265 ||
    h.version!=0x23 || h.device_type!=0x04 ||
    h.ci!=0x7A)return Status::WrongModel;
 if(wmbus_pure::publication::inspect(f,n,h.meter_id)!=
    wmbus_pure::publication::Decision::Allowed)return Status::NotClear;
 // Source-anchored exact original layout: 0C05 total, 4C05 year,
 // 426C billing date, 326C error date, 046D datetime, 0C78 id.
 if(n!=47 || f[12]!=0 || f[13]!=0 || f[14]!=0 ||
    f[15]!=0x0C || f[16]!=0x05 ||
    f[21]!=0x4C || f[22]!=0x05 ||
    f[27]!=0x42 || f[28]!=0x6C ||
    f[31]!=0x32 || f[32]!=0x6C ||
    f[35]!=0x04 || f[36]!=0x6D ||
    f[41]!=0x0C || f[42]!=0x78)
   return Status::BadRecords;
 // Validate complete record stream, not just two convenient offsets.
 standard::Record rows[8]{};size_t count=0;
 if(standard::records(f+15,n-15,rows,8,count)!=standard::Status::Ok ||
    count!=6)return Status::BadRecords;
 Reading candidate{};
 if(!energy(f+15,6,0,candidate.total_kwh) ||
    !energy(f+21,6,1,candidate.target_year_kwh) ||
    candidate.target_year_kwh>candidate.total_kwh)
   return Status::BadRecords;
 out=candidate;return Status::Ok;
}
}} // namespace xmq_portable::qheatv2
