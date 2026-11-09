#pragma once
// Gran Systems GSS/01/02 clear CI78 (application data without TPL header).
// Two original CRC-stripped RF examples 121/159 bytes from
// wmbusmeters/drivers/src/gransystems.xmq GPL-3.0-or-later.
// Unlike CI7A/72, CI78 carries NO TPL security configuration;
// therefore admission is an exact driver/MVT/profile exception,
// not a global security-gate relaxation.
// Never assert status=OK: original 3-phase sample has RTC_ERROR.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "pure_frame_header.h"
#include "standard_dif_vif.h"
namespace xmq_portable { namespace gransystems {
enum class Status:uint8_t {Ok,Invalid,WrongModel,BadRecords};
struct Reading {
 double total_energy_consumption_kwh{0};
 double total_energy_consumption_tariff_1_kwh{0};
 double total_energy_consumption_tariff_2_kwh{0};
};
inline bool read(const uint8_t*f,size_t n,uint8_t dif,
                 uint8_t vif,uint16_t tariff,double &value) {
 standard::Record r[1]{};size_t count=0;
 if(standard::records(f,n,r,1,count)!=standard::Status::Ok ||
    count!=1 || !r[0].numeric || r[0].dif!=dif ||
    r[0].vif!=vif || r[0].tariff!=tariff ||
    r[0].storage || r[0].function || r[0].subunit ||
    r[0].modifier_count || r[0].vif_extension_count)
   return false;
 value=r[0].value*0.001; // source 0x03: 1 Wh per raw unit
 return std::isfinite(value) && value>=0 && value<1.e10;
}
inline Status decode(const uint8_t*f,size_t n,Reading &out) {
 out={};wmbus_pure::Header h{};
 if(wmbus_pure::parse_header(f,n,h)!=wmbus_pure::HeaderStatus::Ok)
    return Status::Invalid;
 if(h.control!=0x44 || h.manufacturer!=0x1E73 ||
    h.version!=0x01 || h.device_type!=0x02 || h.ci!=0x78)
    return Status::WrongModel;
 // Exact source record key offsets of both otherwise different
 // phase-specific profiles (single- and three-phase).
 if((n!=121 && n!=159) ||
    f[11]!=0x04 || f[12]!=0x6D ||
    f[17]!=0x04 || f[18]!=0x03 ||
    f[23]!=0x84 || f[24]!=0x10 || f[25]!=0x03 ||
    f[30]!=0x84 || f[31]!=0x20 || f[32]!=0x03 ||
    f[37]!=0x84 || f[38]!=0x30 || f[39]!=0x03 ||
    f[44]!=0x84 || f[45]!=0x80 || f[46]!=0x10 || f[47]!=0x03 ||
    f[52]!=0x84 || f[53]!=0x01 || f[54]!=0x6D ||
    f[59]!=0x84 || f[60]!=0x01 || f[61]!=0x03 ||
    f[66]!=0x84 || f[67]!=0x11 || f[68]!=0x03 ||
    f[73]!=0x84 || f[74]!=0x21 || f[75]!=0x03 ||
    f[80]!=0x84 || f[81]!=0x31 || f[82]!=0x03 ||
    f[87]!=0x84 || f[88]!=0x81 || f[89]!=0x10 || f[90]!=0x03)
   return Status::BadRecords;
 if(n==121 && (f[95]!=0x04 || f[96]!=0xFD ||
    f[97]!=0x48 || f[114]!=0x04 || f[115]!=0xFD ||
    f[116]!=0x17))
   return Status::BadRecords;
 if(n==159 && (f[95]!=0x84 || f[96]!=0x40 ||
    f[97]!=0xFD || f[98]!=0x48 ||
    f[152]!=0x04 || f[153]!=0xFD || f[154]!=0x17))
   return Status::BadRecords;
 // Entire rest of source fixture is bounded and parsed to rule out
 // invalid/malformed records or truncated application tails.
 standard::Record all[40]{};size_t count=0;
 if(standard::records(f+11,n-11,all,40,count)!=standard::Status::Ok ||
    count<12) return Status::BadRecords;
 Reading r{};
 if(!read(f+17,6,0x04,0x03,0,r.total_energy_consumption_kwh) ||
    !read(f+23,7,0x84,0x03,1,
          r.total_energy_consumption_tariff_1_kwh) ||
    !read(f+30,7,0x84,0x03,2,
          r.total_energy_consumption_tariff_2_kwh))
    return Status::BadRecords;
 if(r.total_energy_consumption_tariff_1_kwh+
    r.total_energy_consumption_tariff_2_kwh>
    r.total_energy_consumption_kwh+0.001)
    return Status::BadRecords;
 out=r;return Status::Ok;
}
}} // namespace xmq_portable::gransystems
