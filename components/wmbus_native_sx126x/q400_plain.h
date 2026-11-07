#pragma once
// Axioma Q400: strict unencrypted CI=7A clear-record subset.
// Original q400.xmq by wmbusmeters (GPL-3.0-or-later).
// Mode-5 post-decrypted NOKEY public fixtures MUST NOT enter this path.
#include <cstddef>
#include <cstdint>
#include <cmath>
#include "standard_dif_vif.h"
#include "native_publication_gate.h"
namespace xmq_portable { namespace q400 {
enum class Status:uint8_t { Ok, Invalid, WrongModel, NotClear, BadRecords, MissingTotal, Duplicate };
struct Reading {
  bool total{false}, flow{false}, temperature{false}, battery{false};
  double total_m3{0}, flow_m3h{0}, temperature_c{0}, battery_pct{0};
};
inline Status decode(const uint8_t *frame,size_t length,Reading &out) {
  out={};
  wmbus_pure::Header h{};
  if(wmbus_pure::parse_header(frame,length,h)!=wmbus_pure::HeaderStatus::Ok)
    return Status::Invalid;
  if(h.manufacturer!=0x0709 || h.device_type!=0x07 ||
      (h.version!=0x01 && h.version!=0x10 && h.version!=0x20) ||
      h.ci!=0x7A) return Status::WrongModel;
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
    if(!r.numeric || r.storage || r.tariff || r.subunit || r.function ||
       r.modifier_count) continue;
    bool *has=nullptr; double *dest=nullptr; double value=0;
    if(!r.vif_extension_count && r.vif>=0x10 && r.vif<=0x17) {
      if(!standard::to_canonical(r.vif,r.value,value)) continue;
      has=&next.total;dest=&next.total_m3;
    } else if(!r.vif_extension_count && r.vif>=0x38 && r.vif<=0x3F) {
      if(!standard::to_canonical(r.vif,r.value,value)) continue;
      has=&next.flow;dest=&next.flow_m3h;
    } else if(!r.vif_extension_count && r.vif>=0x58 && r.vif<=0x5B) {
      if(!standard::to_canonical(r.vif,r.value,value)) continue;
      has=&next.temperature;dest=&next.temperature_c;
    } else if(r.full_vif==0x7D74 && r.vif_extension_count==1) {
      value=r.value;
      has=&next.battery;dest=&next.battery_pct;
    }
    if(has) {
      if(*has || !std::isfinite(value)) return Status::Duplicate;
      *has=true;*dest=value;
    }
  }
  if(!next.total || next.total_m3<0) return Status::MissingTotal;
  out=next;return Status::Ok;
}
}} // namespace xmq_portable::q400
