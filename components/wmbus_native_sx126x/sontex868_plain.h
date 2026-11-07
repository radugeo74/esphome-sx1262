#pragma once
// Sontex868: native strict clear short-TPL HCA/temperature subset.
// Original sontex868.xmq copyright (C) 2020-2022 Fredrik Öhrström,
// GPL-3.0-or-later. No legacy radio/decoder or AES inference.
// Input MUST be DLL CRC-verified and CRC-stripped by receiver.
#include <cstddef>
#include <cstdint>
#include <cmath>
#include "standard_dif_vif.h"
#include "native_publication_gate.h"

namespace xmq_portable { namespace sontex868 {
enum class Status : uint8_t { Ok, Invalid, WrongModel, NotClear, BadRecords, MissingCurrent, Duplicate };
struct Reading {
  bool current{false},previous{false},flow{false},room{false},
       max_flow{false},previous_max_flow{false};
  double current_hca{0},previous_hca{0},flow_c{0},room_c{0},
         max_flow_c{0},previous_max_flow_c{0};
};
inline Status decode(const uint8_t *frame,size_t length,Reading &out) {
  out={};
  wmbus_pure::Header h{};
  if(wmbus_pure::parse_header(frame,length,h)!=
     wmbus_pure::HeaderStatus::Ok) return Status::Invalid;
  if(h.manufacturer!=0x4DEE || h.version!=0x16 ||
     h.device_type!=0x08 || h.ci!=0x7A) return Status::WrongModel;
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
    if(!r.numeric || r.tariff || r.subunit ||
       r.vif_extension_count || r.modifier_count) continue;
    double value=0;
    bool *present=nullptr;double *target=nullptr;
    if(r.vif==0x6E && r.function==0 &&
       (r.storage==0 || r.storage==1)) {
      value=r.value;
      if(r.storage==0) {present=&next.current;target=&next.current_hca;}
      else {present=&next.previous;target=&next.previous_hca;}
    } else if(r.vif>=0x58 && r.vif<=0x5B &&
              (r.function==0 || r.function==1) &&
              (r.storage==0 || r.storage==1)) {
      if(!standard::to_canonical(r.vif,r.value,value)) continue;
      if(r.function==0 && r.storage==0) {
        present=&next.flow;target=&next.flow_c;
      } else if(r.function==1 && r.storage==0) {
        present=&next.max_flow;target=&next.max_flow_c;
      } else if(r.function==1 && r.storage==1) {
        present=&next.previous_max_flow;target=&next.previous_max_flow_c;
      }
    } else if(r.vif>=0x64 && r.vif<=0x67 &&
              r.function==0 && r.storage==0) {
      if(!standard::to_canonical(r.vif,r.value,value)) continue;
      present=&next.room;target=&next.room_c;
    }
    if(present) {
      if(*present || !std::isfinite(value)) return Status::Duplicate;
      *present=true;*target=value;
    }
  }
  if(!next.current) return Status::MissingCurrent;
  out=next;
  return Status::Ok;
}
}} // namespace xmq_portable::sontex868
