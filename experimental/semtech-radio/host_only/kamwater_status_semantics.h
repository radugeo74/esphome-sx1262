#pragma once
// HOST ONLY: Kamwater status and duration rules from original kamwater.xmq.
// No RF/ELL admission or publishing. This is a bounded clear-DIF/VIF reader.
// Status fields 02FF20 / 04FF23 / 04FF24 use different duration bit offsets.
#include <cstddef>
#include <cstdint>
#include <string>
#include "components/wmbus_native_sx126x/standard_dif_vif.h"
namespace xmq_portable { namespace kamwater { namespace status_semantics {
enum class Status : uint8_t { Ok, Invalid, MissingStatus, Ambiguous };
struct Reading {
  std::string status;
  std::string current_status;
  bool has_current_status{false};
  std::string time_dry,time_reversed,time_leaking,time_bursting;
  std::string time_low_ambient_temperature,time_high_ambient_temperature;
  std::string time_flow_above_q4;
  bool modern{false};
  bool hot{false};
};
inline std::string bucket(uint32_t value,unsigned shift) {
  static const char *text[]={
    "","1-8 hours","9-24 hours","2-3 days","4-7 days",
    "8-14 days","15-21 days","22-31 days"
  };
  return text[(value>>shift)&7u];
}
inline std::string common_flags(uint32_t raw,bool modern,bool hot) {
  static const char *flags[]={
    "DRY","REVERSE","LEAK","BURST","TAMPER","LOW_BATTERY",
    "AMBIENT","FLOW_ABOVE_Q4","RESERVED","NO_CONSUMPTION"
  };
  const unsigned maxbit=modern?10u:4u;
  std::string text;
  for(unsigned bit=0;bit<maxbit;++bit) {
    if(bit==8) continue;
    if((raw&(uint32_t{1}<<bit))==0) continue;
    const char *name=flags[bit];
    if(bit==6) name=hot?"HIGH_AMBIENT_TEMPERATURE":
                            "LOW_AMBIENT_TEMPERATURE";
    if(!text.empty()) text+=" ";
    text+=name;
  }
  return text;
}
inline Status decode(const uint8_t *data,size_t length,Reading &out) {
  out={};
  standard::Record records[48]{};
  size_t count=0;
  if(standard::records(data,length,records,48,count)!=standard::Status::Ok)
    return Status::Invalid;
  bool found=false;
  Reading candidate{};
  for(size_t i=0;i<count;++i) {
    const auto &r=records[i];
    if(!r.numeric || r.function!=0 || r.storage!=0 ||
       r.tariff!=0 || r.subunit!=0) continue;
    if(r.full_vif!=0x7F20 && r.full_vif!=0x7F23 &&
       r.full_vif!=0x7F24) continue;
    if(found) return Status::Ambiguous;
    found=true;
    const uint32_t raw=static_cast<uint32_t>(r.raw64);
    const bool modern=r.full_vif!=0x7F20;
    const bool hot=r.full_vif==0x7F24;
    candidate.modern=modern;
    candidate.hot=hot;
    candidate.status=common_flags(raw,modern,hot);
    if(candidate.status.empty()) candidate.status="OK";
    if(!modern) {
      candidate.has_current_status=true;
      candidate.current_status=common_flags(raw,false,false);
    }
    const unsigned first=modern?11u:4u;
    candidate.time_dry=bucket(raw,first);
    candidate.time_reversed=bucket(raw,first+3);
    candidate.time_leaking=bucket(raw,first+6);
    candidate.time_bursting=bucket(raw,first+9);
    if(modern) {
      if(hot) candidate.time_high_ambient_temperature=bucket(raw,23);
      else candidate.time_low_ambient_temperature=bucket(raw,23);
      candidate.time_flow_above_q4=bucket(raw,26);
    }
  }
  if(!found) return Status::MissingStatus;
  out=candidate;
  return Status::Ok;
}
}}} // namespace xmq_portable::kamwater::status_semantics
