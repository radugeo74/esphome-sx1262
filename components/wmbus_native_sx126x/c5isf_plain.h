#pragma once
// Zenner ZRI/88/0D C5ISF strict 230-byte short-TPL Mode0 profile.
// Source: wmbusmeters/drivers/src/c5isf.xmq GPL-3.0-or-later.
// One original clear frame with explicit "DRY ERROR RETURN_SENSOR_INTERRUPTED"
// status; two further NOKEY fixtures have TPL AES Mode5 and are rejected.
// Only unambiguous current/previous-month energy and current volume
// are published. All historical sentinel -2147483648 values and
// invalid 0xFFFF date placeholders stay unpublished.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "native_publication_gate.h"
#include "standard_dif_vif.h"
namespace xmq_portable { namespace c5isf {
enum class Status:uint8_t {Ok,Invalid,WrongModel,NotClear,BadRecords,Missing,Duplicate};
struct Reading {
 double total_energy_consumption_kwh{0};
 double total_volume_m3{0};
 double total_energy_consumption_last_month_kwh{0};
 uint16_t error_flags{0};
 uint8_t tpl_status{0};
};
inline Status decode(const uint8_t *f,size_t n,Reading &out) {
 out={};wmbus_pure::Header h{};
 if(wmbus_pure::parse_header(f,n,h)!=wmbus_pure::HeaderStatus::Ok)
   return Status::Invalid;
 if(h.control!=0x44 || h.manufacturer!=0x6A49 ||
    h.version!=0x88 || h.device_type!=0x0D || h.ci!=0x7A)
   return Status::WrongModel;
 if(wmbus_pure::publication::inspect(f,n,h.meter_id)!=
    wmbus_pure::publication::Decision::Allowed)return Status::NotClear;
 // The public error-state frame has nonzero TPL status 0x02.
 // Fail closed for other layouts/security; never claim numeric
 // decode implies error-free readings.
 if(n!=230 || f[12]!=0x02 || f[13]!=0 || f[14]!=0 ||
    f[15]!=0x2F || f[16]!=0x2F ||
    f[17]!=0x04 || f[18]!=0x06 ||
    f[23]!=0x04 || f[24]!=0x13 ||
    f[29]!=0x02 || f[30]!=0xFD ||
    f[31]!=0x17 || f[34]!=0x84 ||
    f[35]!=0x80 || f[36]!=0x01 || f[37]!=0x06)
   return Status::BadRecords;
 standard::Record rows[64]{};size_t count=0;
 if(standard::records(f+15,n-15,rows,64,count)!=
    standard::Status::Ok)return Status::BadRecords;
 Reading candidate{};unsigned mask=0;
 candidate.error_flags=static_cast<uint16_t>(f[32]|(f[33]<<8));
 candidate.tpl_status=f[12];
 for(size_t i=0;i<count;++i) {
   const auto &r=rows[i];
   if(!r.numeric || r.function || r.tariff || r.subunit ||
      r.vif_extension_count || r.modifier_count)continue;
   unsigned field=3;double value=0;
   if(r.dif==0x04 && r.vif==0x06 && r.storage==0) {
     field=0;value=r.value; // VIF06: 1 kWh
   } else if(r.dif==0x04 && r.vif==0x13 && r.storage==0) {
     field=1;
     if(!standard::to_canonical(0x13,r.value,value))
       return Status::BadRecords;
   } else if(r.vif==0x06 && r.storage==32) {
     field=2;value=r.value; // monthly energy, 1 kWh
   }
   if(field==3)continue;
   if(mask&(1u<<field))return Status::Duplicate;
   if(!std::isfinite(value) || value<0 || value>10000000000.0)
     return Status::BadRecords;
   mask|=1u<<field;
   if(field==0)candidate.total_energy_consumption_kwh=value;
   if(field==1)candidate.total_volume_m3=value;
   if(field==2)candidate.total_energy_consumption_last_month_kwh=value;
 }
 if(mask!=0x07)return Status::Missing;
 if(candidate.total_energy_consumption_last_month_kwh>
    candidate.total_energy_consumption_kwh)return Status::BadRecords;
 out=candidate;return Status::Ok;
}
}} // namespace xmq_portable::c5isf
