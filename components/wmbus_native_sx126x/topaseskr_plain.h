#pragma once
// Integra Topas ESKR: bounded post-AES-decryption short-TPL record subset.
// Original topaseskr.xmq copyright (C) 2017-2022 Fredrik Öhrström
// and Avandorp; GPL-3.0-or-later. Never call on unknown ciphertext.
#include <cstddef>
#include <cstdint>
#include <cmath>
#include "standard_dif_vif.h"
namespace xmq_portable { namespace topaseskr {
enum class Status:uint8_t { Ok, Invalid, BadRecords, Duplicate, Missing };
struct Reading {
  bool total{false},temperature{false},flow{false},year{false};
  double total_m3{0},temperature_c{0},flow_m3h{0},year_m3{0};
};
// The supplied data starts after decrypted 2F2F; it must contain full,
// valid standard DIF/VIF records, never guessed data from an offset.
inline Status decode_clear_application(const uint8_t *application,size_t length,
                                       Reading &out) {
  out={};
  if(!application || length<12 || length>240) return Status::Invalid;
  standard::Record rec[64]{};
  size_t count=0;
  if(standard::records(application,length,rec,64,count)!=standard::Status::Ok)
    return Status::BadRecords;
  Reading next{};
  for(size_t i=0;i<count;++i) {
    const auto &r=rec[i];
    if(!r.numeric || r.function || r.subunit || r.tariff ||
       r.vif_extension_count || r.modifier_count) continue;
    bool *has=nullptr;double *dst=nullptr;double value=0;
    if(r.vif>=0x10 && r.vif<=0x17 &&
       (r.storage==0 || r.storage==1)) {
      if(!standard::to_canonical(r.vif,r.value,value)) continue;
      if(r.storage==0) {has=&next.total;dst=&next.total_m3;}
      else {has=&next.year;dst=&next.year_m3;}
    } else if(r.storage==0 && r.vif>=0x58 && r.vif<=0x5B) {
      if(!standard::to_canonical(r.vif,r.value,value)) continue;
      has=&next.temperature;dst=&next.temperature_c;
    } else if(r.storage==0 && r.vif>=0x38 && r.vif<=0x3F) {
      if(!standard::to_canonical(r.vif,r.value,value)) continue;
      has=&next.flow;dst=&next.flow_m3h;
    }
    if(has) {
      if(*has || !std::isfinite(value)) return Status::Duplicate;
      *has=true;*dst=value;
    }
  }
  if(!next.total || !next.temperature || !next.flow ||
     next.total_m3<0) return Status::Missing;
  out=next;return Status::Ok;
}
}} // namespace xmq_portable::topaseskr
