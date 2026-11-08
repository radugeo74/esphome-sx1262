#pragma once
// B.Meters Hydrodigit BMT/13-15-17/06-07, standard Mode-0 water total.
// Original upstream driver: wmbusmeters/drivers/src/hydrodigit.xmq
// GPL-3.0-or-later. No manufacturer-specific contents, leakage alarm,
// history, date or voltage semantics are inferred from opaque bytes.
// Never decode an AES Mode-5 NOKEY supplied-clear sample as on-air clear.
#include <cstddef>
#include <cstdint>
#include <cmath>
#include "standard_dif_vif.h"
#include "native_publication_gate.h"
namespace xmq_portable { namespace hydrodigit {
enum class Status:uint8_t {Ok,Invalid,WrongModel,NotClear,BadRecords};
struct Reading {double total_m3{0};};
inline bool valid_model(const wmbus_pure::Header &h) {
  return h.manufacturer==0x09B4 &&
    ((h.version==0x13 && (h.device_type==0x06 || h.device_type==0x07)) ||
     (h.version==0x15 && h.device_type==0x07) ||
     (h.version==0x17 && (h.device_type==0x06 || h.device_type==0x07)));
}
inline Status decode(const uint8_t *f,size_t n,Reading &out) {
  out={};wmbus_pure::Header h{};
  if(wmbus_pure::parse_header(f,n,h)!=wmbus_pure::HeaderStatus::Ok)
    return Status::Invalid;
  if(h.control!=0x44 || !valid_model(h) || h.ci!=0x7A)
    return Status::WrongModel;
  if(wmbus_pure::publication::inspect(f,n,h.meter_id)!=
     wmbus_pure::publication::Decision::Allowed)return Status::NotClear;
  // Fixed standard six-byte DIF/VIF total, followed by 4-byte device
  // date/time. The remaining manufacturer tail is opaque and ignored.
  if(n<28 || (n!=34 && n!=71 && n!=74 && n!=77) ||
     f[15]!=0x0C || f[16]!=0x13 ||
     f[21]!=0x04 || f[22]!=0x6D || f[27]!=0x0F)
    return Status::BadRecords;
  standard::Record first[1]{};size_t count=0;
  if(standard::records(f+15,6,first,1,count)!=standard::Status::Ok ||
     count!=1 || !first[0].numeric || first[0].vif!=0x13 ||
     first[0].storage || first[0].function || first[0].tariff ||
     first[0].subunit || first[0].vif_extension_count ||
     first[0].modifier_count)return Status::BadRecords;
  double total=0;
  if(!standard::to_canonical(0x13,first[0].value,total) ||
     !std::isfinite(total) || total<0 || total>100000000)
    return Status::BadRecords;
  out.total_m3=total;return Status::Ok;
}
}} // namespace xmq_portable::hydrodigit
