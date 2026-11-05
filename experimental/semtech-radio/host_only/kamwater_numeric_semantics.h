#pragma once
// HOST ONLY. Upstream Kamwater XMQ numeric field selectors, applied to
// bounded EN13757 DIF/VIF records. Only already-clear applications.
// This is field parity, NOT RF admission, cryptographic verification or
// authorization for sensor publication.
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>
#include <utility>
#include "components/wmbus_native_sx126x/standard_dif_vif.h"
namespace xmq_portable { namespace kamwater { namespace numeric_semantics {
enum class Status : uint8_t { Ok, Invalid, Duplicate, InvalidDate };
struct Field { std::string name; double value; };
struct Reading {
  std::vector<Field> fields;
  bool has_target_date{false};
  standard::Calendar target_date{};
};
inline const char *period(uint32_t storage) {
  switch(storage) {
    case 1:return "month";
    case 2:return "day";
    case 3:return "year";
    default:return nullptr;
  }
}
inline const char *name_for(const standard::Record &r) {
  if(!r.numeric || r.tariff!=0 || r.subunit!=0 || r.vif_extension_count!=0)
    return nullptr;
  const auto unit=standard::canonical_unit(r.vif);
  if(unit==standard::CanonicalUnit::CubicMetersPerHour) {
    if(r.storage==0 && r.function==0) return "flow_m3h";
    if(r.storage==1) {
      if(r.function==1) return "max_flow_last_month_m3h";
      if(r.function==2) return "min_flow_last_month_m3h";
    }
    if(r.storage==2) {
      if(r.function==1) return "max_flow_last_day_m3h";
      if(r.function==2) return "min_flow_last_day_m3h";
    }
    if(r.storage==3 && r.function==1) return "max_flow_last_year_m3h";
  }
  if(unit==standard::CanonicalUnit::Celsius) {
    const bool is_flow=(r.vif>=0x58 && r.vif<=0x5F);
    if(r.function!=1 && r.function!=2) return nullptr;
    const char *when=period(r.storage);
    if(!when) return nullptr;
    if(is_flow) {
      if(r.storage==1) return r.function==1?
          "max_flow_temperature_last_month_c":"min_flow_temperature_last_month_c";
      if(r.storage==2) return r.function==1?
          "max_flow_temperature_last_day_c":"min_flow_temperature_last_day_c";
      return r.function==1?
          "max_flow_temperature_last_year_c":"min_flow_temperature_last_year_c";
    }
    if(r.storage==1) return r.function==1?
        "max_external_temperature_last_month_c":"min_external_temperature_last_month_c";
    if(r.storage==2) return r.function==1?
        "max_external_temperature_last_day_c":"min_external_temperature_last_day_c";
    return r.function==1?
        "max_external_temperature_last_year_c":"min_external_temperature_last_year_c";
  }
  return nullptr;
}
inline Status decode(const uint8_t *data,size_t length,Reading &out) {
  out={};
  standard::Record records[48]{};
  size_t count=0;
  if(standard::records(data,length,records,48,count)!=standard::Status::Ok)
    return Status::Invalid;
  Reading parsed{};
  for(size_t i=0;i<count;++i) {
    const auto &rec=records[i];
    if(rec.numeric && rec.vif==0x6C && rec.vif_extension_count==0 &&
       rec.storage==1 && rec.function==0 &&
       rec.tariff==0 && rec.subunit==0) {
      if(parsed.has_target_date) return Status::Duplicate;
      if(!standard::decode_calendar(rec.raw,false,parsed.target_date))
        return Status::InvalidDate;
      parsed.has_target_date=true;
      continue;
    }
    const char *key=name_for(rec);
    if(!key) continue;
    double converted=0;
    if(!standard::to_canonical(rec.vif,rec.value,converted) ||
       !std::isfinite(converted)) return Status::Invalid;
    for(const auto &f:parsed.fields)
      if(f.name==key) return Status::Duplicate;
    parsed.fields.push_back({key,converted});
  }
  out=std::move(parsed);
  return Status::Ok;
}
}}} // namespace xmq_portable::kamwater::numeric_semantics
