#pragma once
// EMH eHZP/00/02 native conservative short-TPL electricity subset.
// Source: wmbusmeters/drivers/src/ehzp.xmq GPL-3.0-or-later.
// Only source 175-byte Mode0 fixture; ELL CI8C supplied-clear example
// is NOT a verified no-key TPL radio observation and is rejected.
// Three individual standard numeric DIF/VIF records; vendor-specific
// historical / multiple tariff blocks remain unparsed and unpublished.
#include <cstddef>
#include <cstdint>
#include <cmath>
#include "native_publication_gate.h"
#include "standard_dif_vif.h"
namespace xmq_portable { namespace ehzp {
enum class Status:uint8_t {Ok,Invalid,WrongModel,NotClear,BadRecords};
struct Reading {double total_energy_consumption_kwh{0};
 double current_power_consumption_kw{0};
 double total_energy_production_kwh{0};};
inline bool read(const uint8_t *f,size_t n,uint8_t vif,
                 bool backwards,double divisor,double &out) {
 standard::Record r[1]{};size_t count=0;
 if(standard::records(f,n,r,1,count)!=standard::Status::Ok ||
    count!=1 || !r[0].numeric || r[0].dif!=0x07 ||
    r[0].vif!=vif || r[0].storage || r[0].function ||
    r[0].tariff || r[0].subunit || r[0].vif_extension_count!=
        static_cast<uint8_t>(backwards?1:0) ||
    r[0].modifier_count!=static_cast<uint8_t>(backwards?1:0) ||
    (backwards && r[0].modifiers[0]!=0x3C))
   return false;
 out=r[0].value/divisor;
 return std::isfinite(out)&&out>=0&&out<=1.0e10;
}
inline Status decode(const uint8_t *f,size_t n,Reading &out) {
 out={};wmbus_pure::Header h{};
 if(wmbus_pure::parse_header(f,n,h)!=wmbus_pure::HeaderStatus::Ok)
   return Status::Invalid;
 if(h.control!=0x44 || h.manufacturer!=0x15A8 ||
    h.version!=0x00 || h.device_type!=0x02 || h.ci!=0x7A)
   return Status::WrongModel;
 if(wmbus_pure::publication::inspect(f,n,h.meter_id)!=
    wmbus_pure::publication::Decision::Allowed)return Status::NotClear;
 // Source-anchored fixed profile; unsupported MVT and record variants
 // are declined even though passive AUTO may recognize their MVT.
 if(n!=175 || f[12]!=0 || f[13]!=0 || f[14]!=0 ||
    f[15]!=0x2F || f[16]!=0x2F ||
    f[17]!=0x07 || f[18]!=0x00 ||
    f[27]!=0x07 || f[28]!=0x80 || f[29]!=0x3C ||
    f[38]!=0x07 || f[39]!=0x28 ||
    f[48]!=0x04 || f[49]!=0x20 ||
    f[53]!=0x01 || f[54]!=0x01 || f[55]!=0xFD ||
    f[56]!=0x97 || f[57]!=0x1D || f[58]!=0 ||
    f[59]!=0xC1 || f[60]!=0x01 || f[61]!=0x74 ||
    f[70]!=0xC7 || f[71]!=0x01 || f[72]!=0x03 ||
    f[81]!=0xCD || f[82]!=0x01 || f[83]!=0x83 ||
    f[122]!=0xC7 || f[123]!=0x01 || f[124]!=0x83 ||
    f[134]!=0xCD || f[135]!=0x01 || f[136]!=0x83)
   return Status::BadRecords;
 Reading candidate{};
 if(!read(f+17,10,0x00,false,1e6,
          candidate.total_energy_consumption_kwh) ||
    !read(f+27,11,0x00,true,1e6,
          candidate.total_energy_production_kwh) ||
    !read(f+38,10,0x28,false,1e6,
          candidate.current_power_consumption_kw))
   return Status::BadRecords;
 out=candidate;return Status::Ok;
}
}} // namespace xmq_portable::ehzp
