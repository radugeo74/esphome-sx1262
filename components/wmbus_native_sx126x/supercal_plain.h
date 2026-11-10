#pragma once
// Sontex Supercal 739: all 10 fixture-verified CI=7A records.
// GPL-3.0-or-later upstream: supercal.xmq. DLL CRC must be stripped
// and checked by the receiver; never decrypt/guess keys here.
#include <cstddef>
#include <cstdint>
#include <cmath>
#include "standard_dif_vif.h"
#include "native_publication_gate.h"
#include "xmq_generated_supercal_lse08.h"

namespace xmq_portable { namespace supercal {
enum class Status:uint8_t {Ok,Invalid,WrongModel,NotClear,BadRecords,Missing,Duplicate};
struct Reading {
 bool energy{false},volume{false},power{false},flow{false},
      flow_temp{false},return_temp{false};
 double energy_kwh{0},target_kwh{0},volume_m3{0},target_m3{0},
        power_kw{0},flow_m3h{0},flow_c{0},return_c{0};
 standard::Calendar meter_datetime{},target_date{};
};
inline Status decode(const uint8_t *frame,size_t length,Reading &out) {
 out={};wmbus_pure::Header h{};
 if(wmbus_pure::parse_header(frame,length,h)!=wmbus_pure::HeaderStatus::Ok)
   return Status::Invalid;
 if(h.control!=0x44 || h.ci!=0x7A ||
    h.manufacturer!=xmq_generated_pair::supercal::maker ||
    h.version!=xmq_generated_pair::supercal::version ||
    h.device_type!=xmq_generated_pair::supercal::type)
   return Status::WrongModel;
 if(wmbus_pure::publication::inspect(frame,length,h.meter_id)!=
    wmbus_pure::publication::Decision::Allowed)return Status::NotClear;
 using namespace xmq_generated_pair;
 if(length!=supercal::verified_length || frame[12]!=0 ||
    frame[13]!=0 || frame[14]!=0)return Status::BadRecords;
 for(const auto &rule:supercal::rules)
   if(!match(frame,length,rule))return Status::BadRecords;
 standard::Record records[16]{};size_t count=0;
 if(standard::records(frame+15,length-15,records,16,count)!=
    standard::Status::Ok || count!=10)return Status::BadRecords;
 const auto &r=records;
 if(r[0].storage || r[0].vif!=0x6D ||
    r[1].storage!=20 || r[1].vif!=0x6C ||
    r[2].storage || r[2].vif!=0x06 ||
    r[3].storage!=20 || r[3].vif!=0x06 ||
    r[4].storage || r[4].vif!=0x14 ||
    r[5].storage!=20 || r[5].vif!=0x14 ||
    r[6].storage || r[6].vif!=0x59 ||
    r[7].storage || r[7].vif!=0x5D ||
    r[8].storage || r[8].vif!=0x3B ||
    r[9].storage || r[9].vif!=0x2C)return Status::BadRecords;
 for(size_t i=0;i<10;++i)
   if(!r[i].numeric || r[i].function || r[i].tariff || r[i].subunit ||
      r[i].vif_extension_count || r[i].modifier_count)
     return Status::BadRecords;
 Reading next{};
 if(!standard::decode_calendar(r[0].raw,true,next.meter_datetime) ||
    !standard::decode_calendar(r[1].raw,false,next.target_date))
   return Status::BadRecords;
 next.energy_kwh=r[2].value;
 next.target_kwh=r[3].value;
 if(!standard::to_canonical(0x14,r[4].value,next.volume_m3) ||
    !standard::to_canonical(0x14,r[5].value,next.target_m3) ||
    !standard::to_canonical(0x59,r[6].value,next.flow_c) ||
    !standard::to_canonical(0x5D,r[7].value,next.return_c) ||
    !standard::to_canonical(0x3B,r[8].value,next.flow_m3h))
    return Status::BadRecords;
 // EN13757 VIF 0x2C: power 10 W, therefore kW = raw / 100.
 next.power_kw=r[9].value/100.0;
 if(next.energy_kwh<0 || next.target_kwh<0 || next.volume_m3<0 ||
    next.target_m3<0 || next.flow_m3h<0 ||
    !std::isfinite(next.power_kw) || !std::isfinite(next.flow_c) ||
    !std::isfinite(next.return_c) ||
    next.target_kwh>next.energy_kwh || next.target_m3>next.volume_m3)
   return Status::BadRecords;
 next.energy=true;next.volume=true;next.power=true;next.flow=true;
 next.flow_temp=true;next.return_temp=true;
 out=next;return Status::Ok;
}
}} // namespace xmq_portable::supercal
