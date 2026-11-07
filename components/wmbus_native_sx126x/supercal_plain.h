#pragma once
// Sontex Supercal 739: strict, configured, unencrypted CI=7A short-TPL
// numeric subset of original supercal.xmq (GPL-3.0-or-later).
// Called only after verified DLL CRC stripping. Does not decrypt.
#include <cstddef>
#include <cstdint>
#include <cmath>
#include "standard_dif_vif.h"
#include "native_publication_gate.h"
namespace xmq_portable { namespace supercal {
enum class Status : uint8_t { Ok, Invalid, WrongModel, NotClear, BadRecords, Missing, Duplicate };
struct Reading {
  bool energy{false}, volume{false}, power{false}, flow{false},
       flow_temp{false}, return_temp{false};
  double energy_kwh{0}, volume_m3{0}, power_kw{0}, flow_m3h{0},
         flow_c{0}, return_c{0};
};
inline Status decode(const uint8_t *frame, size_t length, Reading &out) {
  out={};
  wmbus_pure::Header h{};
  if(wmbus_pure::parse_header(frame,length,h)!=wmbus_pure::HeaderStatus::Ok)
    return Status::Invalid;
  if(h.manufacturer!=0x4DEE || h.version!=0x1B ||
     h.device_type!=0x04 || h.ci!=0x7A) return Status::WrongModel;
  if(wmbus_pure::publication::inspect(frame,length,h.meter_id)!=
     wmbus_pure::publication::Decision::Allowed) return Status::NotClear;
  if(length<=15) return Status::BadRecords;
  standard::Record rec[64]{};
  size_t count=0;
  if(standard::records(frame+15,length-15,rec,64,count)!=standard::Status::Ok)
    return Status::BadRecords;
  Reading next{};
  for(size_t i=0;i<count;++i) {
    const auto &r=rec[i];
    if(!r.numeric || r.storage || r.tariff || r.subunit ||
       r.function || r.vif_extension_count || r.modifier_count) continue;
    bool *has=nullptr; double *dst=nullptr; double val=0;
    if(r.vif<=0x07) {
      val=r.value*standard::scale10(static_cast<int>(r.vif)-6);
      has=&next.energy; dst=&next.energy_kwh;
    } else if(r.vif>=0x10 && r.vif<=0x17) {
      if(!standard::to_canonical(r.vif,r.value,val)) continue;
      has=&next.volume; dst=&next.volume_m3;
    } else if(r.vif>=0x28 && r.vif<=0x2F) {
      val=r.value*standard::scale10(static_cast<int>(r.vif)-0x2E);
      has=&next.power; dst=&next.power_kw;
    } else if(r.vif>=0x38 && r.vif<=0x3F) {
      if(!standard::to_canonical(r.vif,r.value,val)) continue;
      has=&next.flow; dst=&next.flow_m3h;
    } else if(r.vif>=0x58 && r.vif<=0x5B) {
      if(!standard::to_canonical(r.vif,r.value,val)) continue;
      has=&next.flow_temp; dst=&next.flow_c;
    } else if(r.vif>=0x5C && r.vif<=0x5F) {
      if(!standard::to_canonical(r.vif,r.value,val)) continue;
      has=&next.return_temp; dst=&next.return_c;
    }
    if(has) {
      if(*has || !std::isfinite(val)) return Status::Duplicate;
      *has=true; *dst=val;
    }
  }
  if(!next.energy || !next.volume) return Status::Missing;
  out=next; return Status::Ok;
}
}} // namespace xmq_portable::supercal
