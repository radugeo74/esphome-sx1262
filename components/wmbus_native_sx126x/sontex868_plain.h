#pragma once
// Sontex868: source-verified 176-byte clear short-TPL profile.
// Original driver sontex868.xmq GPL-3.0-or-later. Unrecognized vendor
// units stay unnamed; compact HCA history is raw HEX, not invented data.
// The caller must first validate and remove the on-air DLL CRC.
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "standard_dif_vif.h"
#include "native_publication_gate.h"
#include "xmq_generated_clear_profiles.h"

namespace xmq_portable { namespace sontex868 {
enum class Status : uint8_t {Ok,Invalid,WrongModel,NotClear,BadRecords,MissingCurrent,Duplicate};
struct Reading {
  bool current{false},previous{false},flow{false},room{false},
       max_flow{false},previous_max_flow{false};
  double current_hca{0},previous_hca{0},flow_c{0},room_c{0},
         max_flow_c{0},previous_max_flow_c{0};
  double storage_48_consumption_hca{0},tariff_1_duration_minutes{0},
         tariff_1_cumulation_counter{0},parameter_activation_state_raw{0},
         manufacturer_ff2d_value{0},manufacturer_ff2d_tariff_2_value{0},
         manufacturer_ff2c_raw{0};
  standard::Calendar set_date{},device_datetime{},storage_48_date{},
                     tariff_1_date{},tariff_2_date{};
  char software_version[7]{};
  char compact_profile_hex[107]{};
};
inline uint32_t little_uint(const uint8_t *f,size_t n) {
  uint32_t v=0;
  for(size_t i=0;i<n;++i)v|=static_cast<uint32_t>(f[i])<<(8*i);
  return v;
}
inline bool bcd_ok(const uint8_t *p,size_t count) {
  for(size_t i=0;i<count;++i)
    if((p[i]&15)>9 || (p[i]>>4)>9)return false;
  return true;
}
inline Status decode(const uint8_t *frame,size_t length,Reading &out) {
  out={};
  wmbus_pure::Header h{};
  if(wmbus_pure::parse_header(frame,length,h)!=wmbus_pure::HeaderStatus::Ok)
    return Status::Invalid;
  if(h.control!=0x46 || h.ci!=0x7A ||
     !xmq_generated_clear::match_model(h.manufacturer,h.version,h.device_type,
        xmq_generated_clear::sontex868::models,1))
    return Status::WrongModel;
  if(wmbus_pure::publication::inspect(frame,length,h.meter_id)!=
     wmbus_pure::publication::Decision::Allowed)
    return Status::NotClear;
  const auto &rules=xmq_generated_clear::sontex868::rules;
  const auto &offsets=xmq_generated_clear::sontex868::offsets;
  if(length!=xmq_generated_clear::sontex868::verified_frame_bytes ||
     frame[12]!=0 || frame[13]!=0 || frame[14]!=0)
    return Status::BadRecords;
  for(size_t i=0;i<8;++i)
    if(!xmq_generated_clear::match_rule(frame+offsets[i],
          length-offsets[i],rules[i]))return Status::BadRecords;
  // Validate every additional storage/tariff/vendor record before using
  // its bytes. Fail closed if a later firmware changes the record layout.
  struct Key {size_t offset; uint8_t data[5]; size_t count;};
  static constexpr Key additional[]={
      {39,{0x82,0x88,0x01,0x6C,0},4},
      {45,{0x83,0x88,0x01,0x6E,0},4},
      {52,{0x8D,0x88,0x01,0xEE,0x1E},5},
      {111,{0x05,0xFF,0x2D,0,0},3},
      {118,{0x85,0x20,0xFF,0x2D,0},4},
      {138,{0x83,0x10,0xFD,0x31,0},4},
      {145,{0x82,0x10,0x6C,0,0},3},
      {150,{0x81,0x10,0xFD,0x61,0},4},
      {155,{0x82,0x20,0x6C,0,0},3},
      {160,{0x0B,0xFD,0x0F,0,0},3},
      {166,{0x02,0xFF,0x2C,0,0},3},
      {171,{0x02,0xFD,0x66,0,0},3},
  };
  for(const auto &key:additional)
    if(key.offset+key.count>length ||
       std::memcmp(frame+key.offset,key.data,key.count)!=0)
      return Status::BadRecords;
  if(frame[57]!=53 || !bcd_ok(frame+163,3))
    return Status::BadRecords;
  // Validate full DIF/VIF tail, including the opaque 53-byte compact profile.
  standard::Record records[64]{};
  size_t count=0;
  if(standard::records(frame+15,length-15,records,64,count)!=
     standard::Status::Ok)return Status::BadRecords;
  Reading next{};
  for(size_t i=0;i<count;++i) {
    const auto &r=records[i];
    if(!r.numeric || r.tariff || r.subunit ||
       r.vif_extension_count || r.modifier_count)continue;
    double value=0;
    bool *present=nullptr;double *target=nullptr;
    if(r.vif==0x6E && r.function==0 &&
       (r.storage==0 || r.storage==1)) {
      value=r.value;
      if(r.storage==0){present=&next.current;target=&next.current_hca;}
      else {present=&next.previous;target=&next.previous_hca;}
    } else if(r.vif>=0x58 && r.vif<=0x5B &&
              (r.function==0 || r.function==1) &&
              (r.storage==0 || r.storage==1)) {
      if(!standard::to_canonical(r.vif,r.value,value))continue;
      if(r.function==0 && r.storage==0) {
        present=&next.flow;target=&next.flow_c;
      } else if(r.function==1 && r.storage==0) {
        present=&next.max_flow;target=&next.max_flow_c;
      } else if(r.function==1 && r.storage==1) {
        present=&next.previous_max_flow;target=&next.previous_max_flow_c;
      }
    } else if(r.vif>=0x64 && r.vif<=0x67 &&
              r.function==0 && r.storage==0) {
      if(!standard::to_canonical(r.vif,r.value,value))continue;
      present=&next.room;target=&next.room_c;
    }
    if(present) {
      if(*present || !std::isfinite(value))return Status::Duplicate;
      *present=true;*target=value;
    }
  }
  if(!next.current || !next.previous || !next.flow || !next.room ||
     !next.max_flow || !next.previous_max_flow)
    return Status::MissingCurrent;
  const auto date=[&](size_t off,bool datetime,
                      standard::Calendar &dest)->bool {
    return standard::decode_calendar(
        little_uint(frame+off,datetime?4:2),datetime,dest);
  };
  if(!date(17,true,next.device_datetime) ||
     !date(28,false,next.set_date) ||
     !date(43,false,next.storage_48_date) ||
     !date(148,false,next.tariff_1_date) ||
     !date(158,false,next.tariff_2_date))
    return Status::BadRecords;
  // These are standard or source-verified HCA/scalar fields; exposed
  // under storage/tariff labels rather than guessed billing semantics.
  next.storage_48_consumption_hca=little_uint(frame+49,3);
  next.tariff_1_duration_minutes=little_uint(frame+142,3);
  next.tariff_1_cumulation_counter=frame[154];
  next.parameter_activation_state_raw=little_uint(frame+174,2);
  next.manufacturer_ff2c_raw=little_uint(frame+169,2);
  const auto vendor_float=[&](size_t offset,double &v)->bool {
    const uint32_t bits=little_uint(frame+offset,4);
    float value=0;
    static_assert(sizeof(value)==sizeof(bits),"IEEE754 float required");
    std::memcpy(&value,&bits,sizeof(value));
    if(!std::isfinite(value))return false;
    v=value;
    return true;
  };
  if(!vendor_float(114,next.manufacturer_ff2d_value) ||
     !vendor_float(122,next.manufacturer_ff2d_tariff_2_value))
    return Status::BadRecords;
  if(std::snprintf(next.software_version,sizeof(next.software_version),
      "%02X%02X%02X",static_cast<unsigned>(frame[165]),
      static_cast<unsigned>(frame[164]),
      static_cast<unsigned>(frame[163]))!=6)
    return Status::BadRecords;
  for(size_t i=0;i<53;++i)
    if(std::snprintf(next.compact_profile_hex+i*2,3,"%02X",
       static_cast<unsigned>(frame[58+i]))!=2)
      return Status::BadRecords;
  out=next;
  return Status::Ok;
}
}} // namespace xmq_portable::sontex868
