#pragma once
// Apator ELF: one genuine on-air clear long TPL CI72 sample behind a
// distinct radio DLL sender. Source: wmbusmeters/drivers/src/elf.xmq,
// GPL-3.0-or-later. TPL APA/40/04; sender M 0x8601 V18 T37.
// IMPORTANT: source reports ERROR_FLAGS_2000000; do not publish "OK".
// Source-tested exact 82-byte subset only, not generic heat meter parity.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "bound_long_tpl_plain.h"
#include "standard_dif_vif.h"
namespace xmq_portable { namespace elf {
enum class Status:uint8_t {Ok,Invalid,NotClear,BadRecords};
struct Reading {
 double total_energy_consumption_kwh{0},total_volume_m3{0},
       total_energy_consumption_at_date_kwh{0},
       flow_temperature_c{0},return_temperature_c{0};
};
inline bool one(const uint8_t *frame,size_t offset,size_t length,
                uint8_t dif,uint8_t vif,double factor,double &result) {
 standard::Record r[1]{};size_t count=0;
 if(standard::records(frame+offset,length,r,1,count)!=standard::Status::Ok ||
    count!=1 || !r[0].numeric || r[0].dif!=dif ||
    r[0].vif!=vif || r[0].storage!=static_cast<uint32_t>((dif>>6)&1) ||
    r[0].function || r[0].tariff || r[0].subunit ||
    r[0].vif_extension_count || r[0].modifier_count)
   return false;
 result=r[0].value*factor;
 return std::isfinite(result) && result>=0 && result<=1e10;
}
inline Status decode(const uint8_t *f,size_t n,
                     const char*target,const char*dll,Reading &out) {
 out={};
 const auto gate=bound_long_tpl::inspect(f,n,target,dll,
             0x0601,0x40,0x04,0x8601,0x18,0x37);
 if(gate!=bound_long_tpl::Status::Ok)
   return gate==bound_long_tpl::Status::NotClear
        ? Status::NotClear : Status::Invalid;
 if(n!=82 || f[20]!=0xDA || f[21]!=0 || f[22]!=0x20 ||
    f[23]!=0x02 || f[24]!=0x6C ||
    f[27]!=0x0E || f[28]!=0x01 ||
    f[35]!=0x0C || f[36]!=0x13 ||
    f[41]!=0x0A || f[42]!=0x2D ||
    f[45]!=0x0A || f[46]!=0x5A ||
    f[49]!=0x0A || f[50]!=0x5E ||
    f[53]!=0x44 || f[54]!=0x05 ||
    f[59]!=0x01 || f[60]!=0xFD || f[61]!=0x0C ||
    f[63]!=0x0A || f[64]!=0x65 ||
    f[67]!=0x0A || f[68]!=0xFD || f[69]!=0x47 ||
    f[72]!=0x0A || f[73]!=0x27 ||
    f[76]!=0x04 || f[77]!=0x7F)
   return Status::BadRecords;
 Reading next{};
 // 0E01 BCD6: raw*0.01Wh = raw*0.00001kWh.
 if(!one(f,27,8,0x0E,0x01,0.00001,next.total_energy_consumption_kwh) ||
    !one(f,35,6,0x0C,0x13,0.001,next.total_volume_m3) ||
    !one(f,53,6,0x44,0x05,0.1,next.total_energy_consumption_at_date_kwh) ||
    !one(f,45,4,0x0A,0x5A,0.1,next.flow_temperature_c) ||
    !one(f,49,4,0x0A,0x5E,0.1,next.return_temperature_c))
   return Status::BadRecords;
 if(next.total_energy_consumption_at_date_kwh>
      next.total_energy_consumption_kwh || next.flow_temperature_c>150 ||
    next.return_temperature_c>150)
   return Status::BadRecords;
 out=next;return Status::Ok;
}
}} // namespace xmq_portable::elf
