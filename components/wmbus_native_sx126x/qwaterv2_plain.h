#pragma once
// Qundis QWater V2: conservative, configured-only CI 7A / Mode 0
// fixed standard record subset, not proprietary WalkBy and not AES.
// Upstream source: wmbusmeters/drivers/src/qwaterv2.xmq GPL-3.0-or-later.
// Caller supplies already CRC-verified / CRC-stripped RF and matching DLL ID.
#include <cstddef>
#include <cstdint>
#include <cmath>
#include "standard_dif_vif.h"
#include "native_publication_gate.h"
namespace xmq_portable { namespace qwaterv2 {
enum class Status:uint8_t {Ok,Invalid,WrongModel,NotClear,BadRecords};
struct Reading {
  double total_m3{0},target_year_m3{0},target_m3{0};
};
inline bool valid_mvt(const wmbus_pure::Header &h) {
  const bool qds=h.manufacturer==0x4493 &&
    (h.version==0x16 || h.version==0x17 || h.version==0x18 ||
     h.version==0x19 || h.version==0x1A || h.version==0x1D ||
     h.version==0x35 || h.version==0x36 || h.version==0x33);
  const bool lse=h.manufacturer==0x3265 &&
    (h.version==0x19 || h.version==0x1A || h.version==0x1D ||
     h.version==0x33 || h.version==0x35 || h.version==0x36);
  return (qds||lse) && (h.device_type==0x06 || h.device_type==0x07 ||
                       h.device_type==0x37);
}
inline bool volume(const uint8_t *f,size_t n,uint32_t storage,double &value) {
  standard::Record rec[1]{};size_t count=0;
  if(standard::records(f,n,rec,1,count)!=standard::Status::Ok ||
     count!=1 || !rec[0].numeric || rec[0].vif_extension_count ||
     rec[0].vif!=0x13 || rec[0].storage!=storage ||
     rec[0].function || rec[0].tariff || rec[0].subunit ||
     rec[0].modifier_count)return false;
  return standard::to_canonical(rec[0].vif,rec[0].value,value) &&
         std::isfinite(value) && value>=0 && value<=100000000;
}
inline Status decode(const uint8_t *frame,size_t length,Reading &out) {
  out={};wmbus_pure::Header h{};
  if(wmbus_pure::parse_header(frame,length,h)!=wmbus_pure::HeaderStatus::Ok)
    return Status::Invalid;
  if(h.control!=0x44 || !valid_mvt(h) || h.ci!=0x7A)
    return Status::WrongModel;
  if(wmbus_pure::publication::inspect(frame,length,h.meter_id)!=
     wmbus_pure::publication::Decision::Allowed)return Status::NotClear;
  // Narrow standard CI 7A profile from upstream clear NOKEY frames.
  // CI 78 WalkBy, CI 72 nested/gateway and unrelated variable layouts
  // deliberately rejected rather than interpreted as numeric readings.
  if((length!=58 && length!=60) ||
     frame[12]!=0 || frame[13]!=0 || frame[14]!=0x20 ||
     frame[15]!=0x0C || frame[16]!=0x13 ||
     frame[21]!=0x4C || frame[22]!=0x13 ||
     frame[27]!=0x42 || frame[28]!=0x6C ||
     frame[31]!=0xCC || frame[32]!=0x08 || frame[33]!=0x13 ||
     frame[38]!=0xC2 || frame[39]!=0x08 || frame[40]!=0x6C)
     return Status::BadRecords;
  Reading candidate{};
  if(!volume(frame+15,6,0,candidate.total_m3) ||
     !volume(frame+21,6,1,candidate.target_year_m3) ||
     !volume(frame+31,7,17,candidate.target_m3))
     return Status::BadRecords;
  out=candidate;return Status::Ok;
}
}} // namespace xmq_portable::qwaterv2
