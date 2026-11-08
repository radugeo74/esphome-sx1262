#pragma once
// QCaloric: strict standard DIF/VIF HCA subset on authenticated-by-CRC
// DLL frames with explicitly unencrypted short TPL (not Mode5 or WalkBy).
// No trust in upstream NOKEY as a claim about real RF security.
#include <cstddef>
#include <cstdint>
#include "native_publication_gate.h"
#include "standard_dif_vif.h"
namespace xmq_portable {namespace qcaloric {
enum class Status:uint8_t {Ok,Invalid,WrongModel,Encrypted,Records,Missing,Duplicate};
struct Reading{double current_consumption_hca{0},consumption_at_set_date_hca{0};
 bool has_set{false};};
inline Status decode(const uint8_t *f,size_t n,Reading &out){
 out={};wmbus_pure::Header h{};
 if(wmbus_pure::parse_header(f,n,h)!=wmbus_pure::HeaderStatus::Ok)
   return Status::Invalid;
 const bool qds=h.manufacturer==0x4493 &&
   (h.version==0x34 || h.version==0x35 || h.version==0x36 ||
    h.version==0x3A);
 const bool lse=h.manufacturer==0x3265 &&
   (h.version==0x18 || h.version==0x34 || h.version==0x35);
 if(h.control!=0x44 || !(qds||lse) || h.device_type!=0x08 ||
    h.ci!=0x7A)return Status::WrongModel;
 if(wmbus_pure::publication::inspect(f,n,h.meter_id)!=
    wmbus_pure::publication::Decision::Allowed)return Status::Encrypted;
 if(n<=15 || f[12]!=0)return Status::Records;
 standard::Record rec[48]{};size_t count=0;
 if(standard::records(f+15,n-15,rec,48,count)!=standard::Status::Ok)
   return Status::Records;
 Reading r{};bool current=false;
 for(size_t i=0;i<count;++i){
   const auto &x=rec[i];
   if(!x.numeric || x.vif_extension_count || x.vif!=0x6E ||
       x.tariff || x.subunit || x.function || x.value<0)continue;
   if(x.storage==0){if(current)return Status::Duplicate;
      current=true;r.current_consumption_hca=x.value;}
   if(x.storage==1){if(r.has_set)return Status::Duplicate;
      r.has_set=true;r.consumption_at_set_date_hca=x.value;}
 }
 if(!current)return Status::Missing;
 out=r;return Status::Ok;
}
}}