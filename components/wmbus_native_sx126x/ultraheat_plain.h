#pragma once
// Landis+Gyr Ultraheat: native strict clear short-TPL subset.
// Original ultraheat.xmq copyright (C) 2022-2023 Fredrik Öhrström,
// GPL-3.0-or-later. No legacy radio or AES inference.
// Input MUST be DLL CRC-verified and CRC-stripped by receiver.
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include "standard_dif_vif.h"
#include "native_publication_gate.h"

namespace xmq_portable { namespace ultraheat {
enum class Status : uint8_t { Ok, Invalid, WrongModel, NotClear, BadRecords, MissingHeat, Duplicate };
struct Reading {
  bool heat{false},volume{false},power{false},flow{false},
       flow_temp{false},return_temp{false};
  double heat_kwh{0},volume_m3{0},power_kw{0},flow_m3h{0},
         flow_c{0},return_c{0};
  uint8_t tpl_status{0};
  uint16_t error_flags{0};
  char meter_datetime[24]{};
};
inline Status decode(const uint8_t *frame,size_t length,Reading &out) {
  out={};
  wmbus_pure::Header h{};
  if(wmbus_pure::parse_header(frame,length,h)!=
     wmbus_pure::HeaderStatus::Ok) return Status::Invalid;
  // Only wireless LUG 04/04 and short clear TPL. Upstream #1 is
  // wired M-Bus; #3 is ELL with already-decrypted application.
  if(h.manufacturer!=0x32A7 || h.version!=0x04 ||
     h.device_type!=0x04 || h.ci!=0x7A)
    return Status::WrongModel;
  if(wmbus_pure::publication::inspect(frame,length,h.meter_id)!=
     wmbus_pure::publication::Decision::Allowed) return Status::NotClear;
  // Only source fixture #2 proves clear LUG/04/04 with TPL status
  // 0x20 and synchronous mode 0x2000. #1 is wired; #3 is ELL.
  if(h.control!=0x44 || length!=60 || frame[12]!=0x20 ||
     frame[13]!=0x00 || frame[14]!=0x20 ||
     frame[15]!=0x2F || frame[16]!=0x2F)
    return Status::BadRecords;
  standard::Record rec[64]{};
  size_t count=0;
  if(standard::records(frame+15,length-15,rec,64,count)!=
     standard::Status::Ok) return Status::BadRecords;
  Reading next{};
  next.tpl_status=frame[12];
  bool error_seen=false,datetime_seen=false;
  for(size_t i=0;i<count;++i) {
    const auto &r=rec[i];
    // 02 FD 17 00 00: no error bits in the only valid clear sample.
    if(r.full_vif==0x7D17) {
      if(error_seen || !r.numeric || r.dif!=0x02 ||
         r.vif_extension_count!=1 || r.modifier_count ||
         r.storage || r.tariff || r.subunit || r.function ||
         r.raw64!=0) return Status::BadRecords;
      error_seen=true;
      next.error_flags=static_cast<uint16_t>(r.raw64);
      continue;
    }
    // 06 6D six-byte date+time, with a distinct seconds octet.
    if(r.vif==0x6D) {
      if(datetime_seen || !r.numeric || r.dif!=0x06 ||
         r.vif_extension_count || r.modifier_count ||
         r.storage || r.tariff || r.subunit || r.function)
        return Status::BadRecords;
      const uint64_t raw=r.raw64;
      const unsigned second=static_cast<unsigned>(raw & 0xFFu);
      const unsigned minute=static_cast<unsigned>((raw >> 8) & 0xFFu);
      const unsigned hour=static_cast<unsigned>((raw >> 16) & 0xFFu);
      const uint32_t date_raw=static_cast<uint32_t>((raw >> 24) & 0xFFFFu);
      standard::Calendar date{};
      if(second>=60 || minute>=60 || hour>=24 || (raw >> 40)!=0 ||
         !standard::decode_calendar(date_raw,false,date))
        return Status::BadRecords;
      const int chars=std::snprintf(next.meter_datetime,
           sizeof(next.meter_datetime),"%04d-%02d-%02d %02u:%02u:%02u",
           date.year,date.month,date.day,hour,minute,second);
      if(chars!=19) return Status::BadRecords;
      datetime_seen=true;
      continue;
    }
    if(!r.numeric || r.storage || r.tariff || r.subunit ||
       r.function!=0 || r.vif_extension_count || r.modifier_count) continue;
    double v=0;
    bool *present=nullptr;double *target=nullptr;
    if(r.vif<=0x07) {
      v=r.value*standard::scale10(static_cast<int>(r.vif)-6);
      present=&next.heat;target=&next.heat_kwh;
    } else if(r.vif>=0x10 && r.vif<=0x17) {
      if(!standard::to_canonical(r.vif,r.value,v)) continue;
      present=&next.volume;target=&next.volume_m3;
    } else if(r.vif>=0x28 && r.vif<=0x2F) {
      v=r.value*standard::scale10(static_cast<int>(r.vif)-0x2E);
      present=&next.power;target=&next.power_kw;
    } else if(r.vif>=0x38 && r.vif<=0x3F) {
      if(!standard::to_canonical(r.vif,r.value,v)) continue;
      present=&next.flow;target=&next.flow_m3h;
    } else if(r.vif>=0x58 && r.vif<=0x5B) {
      if(!standard::to_canonical(r.vif,r.value,v)) continue;
      present=&next.flow_temp;target=&next.flow_c;
    } else if(r.vif>=0x5C && r.vif<=0x5F) {
      if(!standard::to_canonical(r.vif,r.value,v)) continue;
      present=&next.return_temp;target=&next.return_c;
    }
    if(present) {
      if(*present || !std::isfinite(v)) return Status::Duplicate;
      *present=true;*target=v;
    }
  }
  // Only six complete measurements plus known zero error and time
  // are source-verified. Reject partial/unknown variants transactionally.
  if(!next.heat || !next.volume || !next.power || !next.flow ||
     !next.flow_temp || !next.return_temp || !error_seen ||
     !datetime_seen) return Status::MissingHeat;
  out=next;
  return Status::Ok;
}
}} // namespace xmq_portable::ultraheat
