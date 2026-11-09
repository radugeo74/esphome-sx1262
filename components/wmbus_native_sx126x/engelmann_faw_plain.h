#pragma once
// Engelmann FAW: one genuine CI72, long TPL Mode0 water sample behind
// a different DLL radio ID, wmbusmeters/drivers/src/engelmann_faw.xmq.
// GPL-3.0-or-later. The second public CI73 example is AES-keyed
// and is not admitted to this native plaintext path.
// The 144-byte original has 15 dated history volume slots and NO
// current total_m3. Never conflate reporting/history values with total.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "bound_long_tpl_plain.h"
#include "standard_dif_vif.h"
namespace xmq_portable { namespace engelmann_faw {
enum class Status:uint8_t {Ok,Invalid,NotClear,BadRecords};
struct Reading {
 double consumption_at_reporting_date_m3{0};
 double consumption_1_months_ago_m3{0};
 double consumption_2_months_ago_m3{0};
};
inline uint32_t u32(const uint8_t*f) {
 return static_cast<uint32_t>(f[0]) |
  (static_cast<uint32_t>(f[1])<<8) |
  (static_cast<uint32_t>(f[2])<<16) |
  (static_cast<uint32_t>(f[3])<<24);
}
inline Status decode(const uint8_t*f,size_t n,
                     const char*target,const char*dll,Reading &out) {
 out={};
 const auto gate=bound_long_tpl::inspect(f,n,target,dll,
             0x14C5,0x00,0x07,0x14C5,0x01,0x07);
 if(gate!=bound_long_tpl::Status::Ok)
   return gate==bound_long_tpl::Status::NotClear
        ? Status::NotClear : Status::Invalid;
 if(n!=144 || f[20]!=0 || f[21]!=0 || f[22]!=0 ||
    f[23]!=0x2F || f[24]!=0x2F ||
    f[25]!=0x42 || f[26]!=0x6C ||
    f[29]!=0x44 || f[30]!=0x13 ||
    f[35]!=0x01 || f[36]!=0xFD || f[37]!=0x17 ||
    f[38]!=0) return Status::BadRecords;
 // Exact original 15 monthly records: storage levels 2 through 16.
 // Validate ALL record headers and values before selecting 2 slots.
 for(unsigned month=1;month<=15;++month){
   const size_t pos=39u+static_cast<size_t>(month-1u)*7u;
   if(f[pos]!=(month%2?0x84:0xC4) ||
      f[pos+1]!=static_cast<uint8_t>((month+1u)/2u) ||
      f[pos+2]!=0x13 ||
      u32(f+pos+3)>100000000u)
      return Status::BadRecords;
 }
 const double at_reporting=u32(f+31)/1000.0;
 const double last_month=u32(f+42)/1000.0;
 const double two_months=u32(f+49)/1000.0;
 if(!std::isfinite(at_reporting) || !std::isfinite(last_month) ||
    !std::isfinite(two_months) ||
    at_reporting>100000000.0 || last_month>100000000.0 ||
    two_months>100000000.0)
   return Status::BadRecords;
 out.consumption_at_reporting_date_m3=at_reporting;
 out.consumption_1_months_ago_m3=last_month;
 out.consumption_2_months_ago_m3=two_months;
 return Status::Ok;
}
}} // namespace xmq_portable::engelmann_faw
