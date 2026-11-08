#pragma once
// LSE HCA model 01/08, original upstream lse_08.xmq (GPL-3.0-or-later).
// Strict original single Mode-0 short-TPL profile, not a generic LSE driver.
// The original supplied-clear fixture has TPL status 0x80; do not claim OK.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "native_publication_gate.h"
#include "standard_dif_vif.h"
namespace xmq_portable { namespace lse_08 {
enum class Status:uint8_t {Ok,Invalid,WrongModel,NotClear,BadRecords,Missing};
struct Reading {double consumption_at_set_date_hca{0}; uint8_t tpl_status{0};};
inline Status decode(const uint8_t *frame,size_t n,Reading &out) {
  out={};wmbus_pure::Header h{};
  if(wmbus_pure::parse_header(frame,n,h)!=wmbus_pure::HeaderStatus::Ok)
    return Status::Invalid;
  if(h.control!=0x44 || h.manufacturer!=0x3265 ||
     h.version!=0x01 || h.device_type!=0x08 || h.ci!=0x7A)
    return Status::WrongModel;
  if(wmbus_pure::publication::inspect(frame,n,h.meter_id)!=
     wmbus_pure::publication::Decision::Allowed)return Status::NotClear;
  // Original 42-byte fixture alone: unknown statuses or packet variants
  // are not interpreted as a valid reading.
  if(n!=42 || frame[12]!=0x80 || frame[13]!=0 || frame[14]!=0 ||
     frame[15]!=0x82 || frame[16]!=0x04 || frame[17]!=0x6C ||
     frame[20]!=0x8B || frame[21]!=0x04 || frame[22]!=0x6E)
    return Status::BadRecords;
  standard::Record rows[12]{};size_t count=0;
  if(standard::records(frame+15,n-15,rows,12,count)!=standard::Status::Ok)
    return Status::BadRecords;
  unsigned hits=0;Reading next{};next.tpl_status=frame[12];
  for(size_t i=0;i<count;++i) {
    const auto &r=rows[i];
    if(!r.numeric || r.vif_extension_count || r.modifier_count ||
       r.function || r.tariff || r.subunit)continue;
    if(r.storage==8 && r.vif==0x6E) {
      if(++hits!=1 || r.value<0 || r.value>10000000 ||
         !std::isfinite(r.value))return Status::BadRecords;
      next.consumption_at_set_date_hca=r.value;
    }
  }
  if(hits!=1)return Status::Missing;
  out=next;return Status::Ok;
}
}} // namespace xmq_portable::lse_08
