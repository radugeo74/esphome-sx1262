#pragma once
// ABB B23 (ABB/20/02) fixed 133-byte source-verified Mode0 profile.
// Source: wmbusmeters/drivers/src/abbb23.xmq GPL-3.0-or-later.
// Only total imported energy is currently proven. No speculative
// electricity tariffs, power or vendor alarm/state interpretation.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "native_publication_gate.h"
#include "standard_dif_vif.h"
namespace xmq_portable { namespace abbb23 {
enum class Status:uint8_t {Ok,Invalid,WrongModel,NotClear,BadRecords};
struct Reading {double total_energy_consumption_kwh{0};};
inline Status decode(const uint8_t *f,size_t n,Reading &out) {
 out={};wmbus_pure::Header h{};
 if(wmbus_pure::parse_header(f,n,h)!=wmbus_pure::HeaderStatus::Ok)
   return Status::Invalid;
 if(h.control!=0x44 || h.manufacturer!=0x0442 ||
    h.version!=0x20 || h.device_type!=0x02 || h.ci!=0x7A)
   return Status::WrongModel;
 if(wmbus_pure::publication::inspect(f,n,h.meter_id)!=
    wmbus_pure::publication::Decision::Allowed)return Status::NotClear;
 // The public ABB B23 sample has TPL status 0, 0x20 no-key
 // algorithm block, and a fixed 0E/84 00 energy record.
 if(n!=133 || f[12]!=0 || f[13]!=0 || f[14]!=0x20 ||
    f[15]!=0x0E || f[16]!=0x84 || f[17]!=0x00 ||
    f[24]!=0x04 || f[25]!=0xFF || f[26]!=0xA0 ||
    f[56]!=0x07 || f[57]!=0xFF || f[58]!=0xA6 ||
    f[68]!=0x07 || f[69]!=0xFF || f[70]!=0xA7 ||
    f[80]!=0x07 || f[81]!=0xFF || f[82]!=0xA8 ||
    f[92]!=0x07 || f[93]!=0xFF || f[94]!=0xA9 ||
    f[104]!=0x0D || f[105]!=0xFD || f[106]!=0x8E ||
    f[116]!=0x0D || f[117]!=0xFF || f[118]!=0xAA ||
    f[132]!=0x1F)
   return Status::BadRecords;
 standard::Record rows[24]{};size_t count=0;
 if(standard::records(f+15,n-15,rows,24,count)!=standard::Status::Ok ||
    count<9)
   return Status::BadRecords;
 unsigned hits=0;double energy=0;
 for(size_t i=0;i<count;++i) {
   const auto&r=rows[i];
   if(r.dif!=0x0E || r.vif!=0x04 || r.storage ||
      r.function || r.tariff || r.subunit ||
      r.vif_extension_count!=1 || r.modifier_count!=1 ||
      r.modifiers[0]!=0 || !r.numeric)
      continue;
   if(++hits!=1)return Status::BadRecords;
   // VIF 0x04 => 10 Wh / raw unit = 0.01 kWh.
   energy=r.value*0.01;
   if(!std::isfinite(energy) || energy<0 || energy>1e10)
      return Status::BadRecords;
 }
 if(hits!=1)return Status::BadRecords;
 out.total_energy_consumption_kwh=energy;
 return Status::Ok;
}
}} // namespace xmq_portable::abbb23
