#pragma once
// Maddalena MAD/01/07 strict public Mode-0 79-byte water application.
// Source: wmbusmeters/drivers/src/maddalena.xmq GPL-3.0-or-later.
// Upstream detect.mvt also lists MAD/01/06 but no original fixture
// confirms its application layout: this narrow decoder rejects it.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "standard_dif_vif.h"
#include "native_publication_gate.h"
namespace xmq_portable { namespace maddalena {
enum class Status:uint8_t {Ok,Invalid,WrongModel,NotClear,BadRecords,Missing,Duplicate};
struct Reading {double total_m3{0},target_m3{0};};
inline Status decode(const uint8_t *frame,size_t n,Reading &out) {
 out={};wmbus_pure::Header h{};
 if(wmbus_pure::parse_header(frame,n,h)!=wmbus_pure::HeaderStatus::Ok)
   return Status::Invalid;
 if(h.control!=0x44 || h.manufacturer!=0x3424 ||
    h.version!=0x01 || h.device_type!=0x07 || h.ci!=0x7A)
   return Status::WrongModel;
 if(wmbus_pure::publication::inspect(frame,n,h.meter_id)!=
    wmbus_pure::publication::Decision::Allowed)return Status::NotClear;
 // Original sample: two fill bytes, 04 13 total, 04 6D datetime,
 // storage-1 44 13 billing, 42 6C date, storage-2 history follows.
 if(n!=79 || frame[12]!=0 || frame[13]!=0 || frame[14]!=0x20 ||
    frame[15]!=0x2F || frame[16]!=0x2F ||
    frame[17]!=0x04 || frame[18]!=0x13 ||
    frame[23]!=0x04 || frame[24]!=0x6D ||
    frame[44]!=0x44 || frame[45]!=0x13 ||
    frame[50]!=0x42 || frame[51]!=0x6C)
   return Status::BadRecords;
 // The manufacturer/fabrication field 0E78 carries a terminal FF
 // identifier sentinel. Its bytes are NOT a valid numeric BCD value,
 // so do not force the generic numeric-record parser through them.
 // Instead admit only the source-verified fixed layout, then decode
 // the two individual standard 04/44 13 volume records independently.
 if(frame[29]!=0x04 || frame[30]!=0xFD || frame[31]!=0x17 ||
    frame[36]!=0x0E || frame[37]!=0x78 || frame[43]!=0xFF ||
    frame[54]!=0x84 || frame[55]!=0x01 || frame[56]!=0x13 ||
    frame[61]!=0x82 || frame[62]!=0x01 || frame[63]!=0x6C ||
    frame[72]!=0xC4 || frame[73]!=0x01 || frame[74]!=0x6D)
   return Status::BadRecords;
 const auto read_volume=[&](size_t offset,uint32_t storage,double &value)->bool {
   standard::Record record[1]{};size_t count=0;
   if(standard::records(frame+offset,6,record,1,count)!=standard::Status::Ok ||
      count!=1 || !record[0].numeric || record[0].vif!=0x13 ||
      record[0].storage!=storage || record[0].function ||
      record[0].tariff || record[0].subunit ||
      record[0].vif_extension_count || record[0].modifier_count)
      return false;
   return standard::to_canonical(0x13,record[0].value,value) &&
          std::isfinite(value) && value>=0 && value<=100000000;
 };
 Reading next{};
 if(!read_volume(17,0,next.total_m3) ||
    !read_volume(44,1,next.target_m3))return Status::BadRecords;
 if(next.target_m3>next.total_m3)return Status::BadRecords;
 out=next;return Status::Ok;
}
}} // namespace xmq_portable::maddalena
