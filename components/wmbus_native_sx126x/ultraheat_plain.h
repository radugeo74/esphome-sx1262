#pragma once
// Landis+Gyr Ultraheat: native strict clear short-TPL subset.
// Original ultraheat.xmq copyright (C) 2022-2023 Fredrik Öhrström,
// GPL-3.0-or-later. No legacy radio or AES inference.
// Input MUST be DLL CRC-verified and CRC-stripped by receiver.
#include <cstddef>
#include <cstdint>
#include <cmath>
#include "standard_dif_vif.h"
#include "native_publication_gate.h"

namespace xmq_portable { namespace ultraheat {
enum class Status : uint8_t { Ok, Invalid, WrongModel, NotClear, BadRecords, MissingHeat, Duplicate };
struct Reading {
  bool heat{false},volume{false},power{false},flow{false},
       flow_temp{false},return_temp{false};
  double heat_kwh{0},volume_m3{0},power_kw{0},flow_m3h{0},
         flow_c{0},return_c{0};
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
  if(length<=15) return Status::BadRecords;
  standard::Record rec[64]{};
  size_t count=0;
  if(standard::records(frame+15,length-15,rec,64,count)!=
     standard::Status::Ok) return Status::BadRecords;
  Reading next{};
  for(size_t i=0;i<count;++i) {
    const auto &r=rec[i];
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
  if(!next.heat) return Status::MissingHeat;
  out=next;
  return Status::Ok;
}
}} // namespace xmq_portable::ultraheat
