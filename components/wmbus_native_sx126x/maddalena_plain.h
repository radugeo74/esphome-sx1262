#pragma once
// MAD/01/07 Maddalena: public CC0 XMQ fixture, complete clear record map.
// MAD/01/06 is metadata-only: no verified fixture means no RF admission.
// Numbers, calendar records and fabrication ID retain upstream semantics.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include "standard_dif_vif.h"
#include "native_publication_gate.h"
#include "xmq_generated_clear_profiles.h"
namespace xmq_portable { namespace maddalena {
enum class Status:uint8_t {Ok,Invalid,WrongModel,NotClear,BadRecords,Missing,Duplicate};
struct Reading {
  double total_m3{0},target_m3{0},storage_2_volume_m3{0},
         storage_3_max_flow_m3h{0};
  standard::Calendar target_date{},meter_datetime{},storage_2_date{},
                     storage_3_datetime{};
  char fabrication_no[13]{};
};
inline bool bcd(uint8_t value){
  return (value&15)<=9 && (value>>4)<=9;
}
inline uint32_t raw32(const uint8_t *p,size_t n){
  uint32_t result=0;
  for(size_t i=0;i<n;++i)result|=static_cast<uint32_t>(p[i])<<(i*8);
  return result;
}
inline Status decode(const uint8_t *frame,size_t n,Reading &out) {
  out={};wmbus_pure::Header h{};
  if(wmbus_pure::parse_header(frame,n,h)!=wmbus_pure::HeaderStatus::Ok)
    return Status::Invalid;
  if(h.control!=0x44 || h.ci!=0x7A || h.device_type!=0x07 ||
     !xmq_generated_clear::match_model(h.manufacturer,h.version,h.device_type,
         xmq_generated_clear::maddalena::models,2))
    return Status::WrongModel;
  if(wmbus_pure::publication::inspect(frame,n,h.meter_id)!=
     wmbus_pure::publication::Decision::Allowed)return Status::NotClear;
  const auto &rules=xmq_generated_clear::maddalena::rules;
  const auto &offsets=xmq_generated_clear::maddalena::offsets;
  if(n!=xmq_generated_clear::maddalena::verified_frame_bytes ||
     frame[12]!=0 || frame[13]!=0 || frame[14]!=0x20 ||
     frame[15]!=0x2F || frame[16]!=0x2F)return Status::BadRecords;
  for(size_t i=0;i<6;++i)
    if(!xmq_generated_clear::match_rule(frame+offsets[i],n-offsets[i],
                                         rules[i]))return Status::BadRecords;
  // Storage 2 and 3: standard record signatures; no fabricated billing tags.
  static constexpr uint8_t extra_keys[][3]={
      {0x84,0x01,0x13},{0x82,0x01,0x6C},
      {0xD3,0x01,0x3B},{0xC4,0x01,0x6D}};
  static constexpr size_t extra_at[]={54,61,66,72};
  for(size_t i=0;i<4;++i)
    for(size_t j=0;j<3;++j)
      if(frame[extra_at[i]+j]!=extra_keys[i][j])return Status::BadRecords;
  // Fabrication 0E78 contains five BCD bytes and terminal 0xFF sentinel,
  // not a standard fully numeric six-byte BCD value.
  if(frame[43]!=0xFF)return Status::BadRecords;
  for(size_t i=38;i<43;++i)
    if(!bcd(frame[i]))return Status::BadRecords;
  // Only the zero-flags example is publicly validated: no false OK alarms.
  if(frame[32] || frame[33] || frame[34] || frame[35])
    return Status::BadRecords;
  const auto volume=[&](size_t offset,size_t size,
                        uint32_t storage,double &value)->bool {
    standard::Record rec[1]{};size_t count=0;
    if(standard::records(frame+offset,size,rec,1,count)!=standard::Status::Ok ||
       count!=1 || !rec[0].numeric || rec[0].vif!=0x13 ||
       rec[0].storage!=storage || rec[0].function ||
       rec[0].tariff || rec[0].subunit ||
       rec[0].vif_extension_count || rec[0].modifier_count)return false;
    return standard::to_canonical(0x13,rec[0].value,value) &&
           std::isfinite(value) && value>=0 && value<=100000000;
  };
  Reading next{};
  if(!volume(17,6,0,next.total_m3) ||
     !volume(44,6,1,next.target_m3) ||
     !volume(54,7,2,next.storage_2_volume_m3) ||
     next.target_m3>next.total_m3 ||
     next.storage_2_volume_m3>next.total_m3)
    return Status::BadRecords;
  standard::Record flow[1]{};size_t count=0;
  if(standard::records(frame+66,6,flow,1,count)!=standard::Status::Ok ||
     count!=1 || !flow[0].numeric || flow[0].dif!=0xD3 ||
     flow[0].storage!=3 || flow[0].function!=1 ||
     flow[0].vif!=0x3B || flow[0].tariff || flow[0].subunit ||
     !standard::to_canonical(flow[0].vif,flow[0].value,
          next.storage_3_max_flow_m3h) ||
     next.storage_3_max_flow_m3h<0)
    return Status::BadRecords;
  const auto read_date=[&](size_t offset,bool datetime,
                           standard::Calendar &value)->bool {
    return standard::decode_calendar(raw32(frame+offset,datetime?4:2),
                                     datetime,value);
  };
  if(!read_date(25,true,next.meter_datetime) ||
     !read_date(52,false,next.target_date) ||
     !read_date(64,false,next.storage_2_date) ||
     !read_date(75,true,next.storage_3_datetime))
    return Status::BadRecords;
  for(size_t i=0;i<6;++i)
    if(std::snprintf(next.fabrication_no+2*i,3,"%02X",
           static_cast<unsigned>(frame[43-i]))!=2)
      return Status::BadRecords;
  out=next;
  return Status::Ok;
}
}} // namespace xmq_portable::maddalena
