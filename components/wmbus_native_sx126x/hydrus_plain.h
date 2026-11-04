#pragma once
// Diehl Hydrus standard-record interpretation from an already plaintext
// application; the original hydrus.xmq is kept unchanged in drivers/.
// No AES, manufacturer-specific transformations or implicit decryption.
#include <cstddef>
#include <cstdint>
#include "standard_dif_vif.h"
#include "pure_frame_header.h"
#include "tpl_security_probe.h"
#include "calculate_subset.h"
namespace xmq_portable { namespace hydrus {
enum class Status : uint8_t { Ok, Invalid, Unsupported, Encrypted, MissingFields, Capacity };
struct DateField { const char *name; standard::Calendar value; };
inline bool add(CalcValue *out, size_t capacity, size_t &n, const char *name, double value) {
  if (n>=capacity) return false;
  out[n++] = {name,value};
  return true;
}
inline Status decode_clear_application(const uint8_t *payload, size_t size,
                                       CalcValue *out, size_t capacity, size_t &out_count,
                                       DateField *dates = nullptr, size_t date_capacity = 0,
                                       size_t *date_count = nullptr) {
  out_count = 0;
  if (date_count) *date_count = 0;
  standard::Record rec[48]{};
  size_t n=0;
  auto result=standard::records(payload,size,rec,48,n);
  if (result!=standard::Status::Ok)
    return result==standard::Status::Capacity?Status::Capacity:Status::Invalid;
  bool total=false;
  for (size_t i=0;i<n;i++) {
    const auto &r=rec[i];
    if (!r.numeric) continue;
    // Standard EN13757 Date G and DateTime fields. These are not
    // floating-point sensor values and must remain typed dates.
    if ((r.vif==0x6C || r.vif==0x6D) &&
        r.vif_extension_count==0 && r.function==0 &&
        r.tariff==0 && r.subunit==0) {
      const char *name = nullptr;
      if (r.vif==0x6C && r.storage==1) name="at_date";
      else if (r.vif==0x6D && (r.storage==3 || r.storage==8))
        name="target_datetime";
      else if (r.vif==0x6D && r.storage==0) name="meter_datetime";
      if (name && dates && date_count) {
        standard::Calendar calendar{};
        if (standard::decode_calendar(r.raw, r.vif==0x6D, calendar)) {
          if (*date_count >= date_capacity) { out_count=0; *date_count=0; return Status::Capacity; }
          dates[(*date_count)++] = {name,calendar};
        }
      }
      continue;
    }
    const int v=r.vif;
    const int storage=r.storage;
    const char *field=nullptr;
    double value=r.value;
    if (r.subunit!=0) continue; // Do not map another subunit as the main meter.
    if (v>=0x10 && v<=0x17) {
      value*=standard::scale10(v-0x16);
      if (r.vif_extension_count == 1 && r.vif_extension == 0x3C &&
          r.tariff==0 && r.function==0 && storage==0) {
        field="backward_m3";
        if (!add(out,capacity,out_count,field,value)) { out_count=0; return Status::Capacity; }
        continue;
      }
      // Any other volume VIFE is not a normal total measurement.
      if (r.vif_extension_count != 0) continue;
      if(storage==0 && r.tariff==0 && r.function==0) { field="total_m3"; total=true; }
      else if(storage==1 && r.tariff==0 && r.function==0) field="total_at_date_m3";
      else if((storage==3 || storage==8) && r.tariff==0 && r.function==0) field="target_m3";
      else if(storage==0 && r.tariff==1 && r.function==0) field="total_tariff1_m3";
      else if(storage==0 && r.tariff==2 && r.function==0) field="total_tariff2_m3";
    } else if(v>=0x38 && v<=0x3F && r.vif_extension_count==0 &&
              r.tariff==0) {
      value*=standard::scale10(v-0x3E);
      if(storage==0 && r.function==0) field="flow_m3h";
      else if(storage==8 && r.function==1) field="max_flow_m3h";
      else if(storage==8 && r.function==2) field="min_flow_m3h";
    } else if(v>=0x58 && v<=0x5B && r.vif_extension_count==0 &&
              r.tariff==0) {
      value*=standard::scale10(v-0x5B);
      if(storage==0 && r.function==0) field="flow_temperature_c";
    } else if(v>=0x64 && v<=0x67 && r.vif_extension_count==0 &&
              r.tariff==0) {
      value*=standard::scale10(v-0x67);
      if(storage==0 && r.function==0) field="external_temperature_c";
    } else if(v==0x7D && r.vif_extension_count==1 &&
              r.vif_extension==0x74 && r.function==0 &&
              r.tariff==0 && storage==0) {
      value/=365.2425;
      field="remaining_battery_life_y";
    }
    if(field && !add(out,capacity,out_count,field,value)) {
      out_count=0; return Status::Capacity;
    }
  }
  if(!total) {out_count=0;return Status::MissingFields;}
  return Status::Ok;
}
// Strict raw RF path: only cleartext CI=7A/72 with a documented security
// configuration of zero. Official underscore fixtures may contain a clear
// *post-decryption* payload and must be tested via decode_clear_application.
inline Status decode_plain_frame(const uint8_t *frame, size_t size,
                                 CalcValue *out, size_t capacity, size_t &count) {
  count=0;
  wmbus_pure::Header h{};
  if(wmbus_pure::parse_header(frame,size,h)!=wmbus_pure::HeaderStatus::Ok)
    return Status::Invalid;
  const bool dme=h.manufacturer==0x11A5;
  const bool hyd=h.manufacturer==0x2324;
  if(!dme && !hyd) return Status::Unsupported;
  size_t at=0;
  if(h.ci==0x7A) at=15;
  else if(h.ci==0x72) at=23;
  else return Status::Unsupported;
  if(size<at) return Status::Invalid;
  const auto security = wmbus_pure::tpl_security::inspect(frame,size);
  if (security.state != wmbus_pure::tpl_security::State::NotEncrypted)
    return Status::Encrypted; // mode zero only, no key or AES supported
  return decode_clear_application(frame+at,size-at,out,capacity,count);
}
} }
