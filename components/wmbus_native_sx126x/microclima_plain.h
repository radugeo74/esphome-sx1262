#pragma once
// Maddalena Microclima heat meter MAD/00/04. Bounded original CI7A
// short-TPL Mode0, 74 and 165 bytes, GPL-3.0-or-later upstream
// wmbusmeters/drivers/src/microclima.xmq.
// Only published source-proven current energy and volume and optional
// flow/return temperatures: no guessed historical dates/status.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "native_publication_gate.h"
#include "standard_dif_vif.h"
namespace xmq_portable { namespace microclima {
enum class Status:uint8_t {Ok,Invalid,WrongModel,NotClear,BadRecords,Missing,Duplicate};
struct Reading {
 double total_energy_consumption_kwh{0},total_volume_m3{0};
 double flow_temperature_c{0},return_temperature_c{0};
 bool has_flow_temperature{false},has_return_temperature{false};
};
inline Status decode(const uint8_t *f,size_t n,Reading &out) {
 out={};wmbus_pure::Header h{};
 if(wmbus_pure::parse_header(f,n,h)!=wmbus_pure::HeaderStatus::Ok)
   return Status::Invalid;
 if(h.control!=0x44 || h.manufacturer!=0x3424 ||
    h.version!=0 || h.device_type!=0x04 || h.ci!=0x7A)
   return Status::WrongModel;
 if(wmbus_pure::publication::inspect(f,n,h.meter_id)!=
    wmbus_pure::publication::Decision::Allowed)return Status::NotClear;
 if((n!=74 && n!=165) || f[12]!=0 || f[13]!=0 ||
    f[14]!=0x20 || f[15]!=0x2F || f[16]!=0x2F ||
    f[17]!=0x04 || f[18]!=0x6D ||
    f[23]!=0x04 || f[24]!=0x06)
    return Status::BadRecords;
 if(n==74 && (f[29]!=0x01 || f[30]!=0xFD || f[31]!=0x17 ||
              f[32]!=0 || f[33]!=0x04 || f[34]!=0x13 ||
              f[39]!=0x04 || f[40]!=0x3B ||
              f[45]!=0x04 || f[46]!=0x2B ||
              f[51]!=0x02 || f[52]!=0x5B ||
              f[55]!=0x02 || f[56]!=0x5F ||
              f[59]!=0x02 || f[60]!=0x61))
    return Status::BadRecords;
 if(n==165 && (f[29]!=0x04 || f[30]!=0x13 ||
               f[35]!=0x01 || f[36]!=0xFD || f[37]!=0x17 ||
               f[38]!=0 || f[39]!=0x42 || f[40]!=0x6C ||
               f[43]!=0x44 || f[44]!=0x06 ||
               f[154]!=0x03 || f[155]!=0xFD || f[156]!=0x0C))
    return Status::BadRecords;
 standard::Record rows[40]{};size_t count=0;
 if(standard::records(f+15,n-15,rows,40,count)!=standard::Status::Ok)
    return Status::BadRecords;
 Reading r{};unsigned mask=0;
 for(size_t i=0;i<count;++i) {
    const auto &v=rows[i];
    if(!v.numeric || v.function || v.tariff || v.subunit ||
       v.vif_extension_count || v.modifier_count || v.storage!=0)
       continue;
    unsigned field=4;double value=0;
    if(v.dif==0x04 && v.vif==0x06) {
      field=0;value=v.value; // 1 kWh
    } else if(v.dif==0x04 && v.vif==0x13) {
      field=1;
      if(!standard::to_canonical(v.vif,v.value,value))
        return Status::BadRecords;
    } else if(n==74 && v.dif==0x02 && v.vif==0x5B) {
      field=2;
      if(!standard::to_canonical(v.vif,v.value,value))
        return Status::BadRecords;
    } else if(n==74 && v.dif==0x02 && v.vif==0x5F) {
      field=3;
      if(!standard::to_canonical(v.vif,v.value,value))
        return Status::BadRecords;
    }
    if(field==4)continue;
    if(mask & (1u<<field))return Status::Duplicate;
    if(!std::isfinite(value) || value < (field>=2?-60.0:0.0) ||
       value > (field>=2?150.0:1e10))return Status::BadRecords;
    mask|=1u<<field;
    if(field==0)r.total_energy_consumption_kwh=value;
    else if(field==1)r.total_volume_m3=value;
    else if(field==2){r.has_flow_temperature=true;r.flow_temperature_c=value;}
    else {r.has_return_temperature=true;r.return_temperature_c=value;}
 }
 if((mask&3)!=3 || (n==74 && (mask&12)!=12))
   return Status::Missing;
 out=r;return Status::Ok;
}
}} // namespace xmq_portable::microclima
