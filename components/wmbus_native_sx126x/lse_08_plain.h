#pragma once
// LSE/01/08: the entire source-verified 42-byte clear-TPL fixture.
// Original lse_08.xmq GPL-3.0-or-later, with TPL status 0x80.
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include "standard_dif_vif.h"
#include "native_publication_gate.h"
#include "xmq_generated_supercal_lse08.h"

namespace xmq_portable { namespace lse_08 {
enum class Status:uint8_t {Ok,Invalid,WrongModel,NotClear,BadRecords,Missing};
struct Reading {
 double consumption_at_set_date_hca{0},duration_since_readout_h{0};
 uint8_t tpl_status{0};
 standard::Calendar set_date{},device_datetime{};
 char model_version[3]{};
};
inline Status decode(const uint8_t *frame,size_t n,Reading &out) {
 out={};wmbus_pure::Header h{};
 if(wmbus_pure::parse_header(frame,n,h)!=wmbus_pure::HeaderStatus::Ok)
    return Status::Invalid;
 if(h.control!=0x44 || h.ci!=0x7A ||
    h.manufacturer!=xmq_generated_pair::lse_08::maker ||
    h.version!=xmq_generated_pair::lse_08::version ||
    h.device_type!=xmq_generated_pair::lse_08::type)
    return Status::WrongModel;
 if(wmbus_pure::publication::inspect(frame,n,h.meter_id)!=
    wmbus_pure::publication::Decision::Allowed)return Status::NotClear;
 if(n!=xmq_generated_pair::lse_08::verified_length ||
    frame[12]!=0x80 || frame[13]!=0 || frame[14]!=0)
    return Status::BadRecords;
 for(const auto &rule:xmq_generated_pair::lse_08::rules)
   if(!xmq_generated_pair::match(frame,n,rule))return Status::BadRecords;
 standard::Record rows[8]{};size_t count=0;
 if(standard::records(frame+15,n-15,rows,8,count)!=standard::Status::Ok ||
    count!=5)return Status::BadRecords;
 if(rows[0].storage!=8 || rows[0].vif!=0x6C ||
    rows[1].storage!=8 || rows[1].vif!=0x6E ||
    rows[2].storage || rows[2].vif!=0x6D ||
    rows[3].storage || rows[3].full_vif!=0x7D2C ||
    rows[3].modifier_count!=1 || rows[3].modifiers[0]!=0x7E ||
    rows[4].storage || rows[4].full_vif!=0x7D0C)
    return Status::BadRecords;
 for(size_t i=0;i<count;++i)
   if(!rows[i].numeric || rows[i].function || rows[i].tariff ||
      rows[i].subunit || !std::isfinite(rows[i].value))
     return Status::BadRecords;
 Reading next{};
 if(!standard::decode_calendar(rows[0].raw,false,next.set_date) ||
    !standard::decode_calendar(rows[2].raw,true,next.device_datetime))
    return Status::BadRecords;
 if(rows[1].value<0 || rows[1].value>10000000 ||
    rows[3].value<0 || rows[3].value>65535)
    return Status::BadRecords;
 next.consumption_at_set_date_hca=rows[1].value;
 // Original source applies the "FutureValue" combinable to elapsed
 // readout seconds, converting the 0x2301 field to 2.489167 hours.
 next.duration_since_readout_h=rows[3].value/3600.0;
 if(std::snprintf(next.model_version,sizeof(next.model_version),"%02X",
       static_cast<unsigned>(rows[4].raw))!=2)
    return Status::BadRecords;
 next.tpl_status=frame[12]; // raw 128 must not be represented as OK.
 out=next;return Status::Ok;
}
}} // namespace xmq_portable::lse_08
