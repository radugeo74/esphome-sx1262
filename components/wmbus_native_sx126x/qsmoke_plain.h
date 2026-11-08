#pragma once
// Qundis QSmoke: narrowly validated, genuinely unencrypted CI=7A profile.
// Never treat transport status as a real-time fire alarm. Smoke detectors
// must retain their certified local audible alarm, independently of HA.
// Copyright reference upstream wmbusmeters qsmoke.xmq GPL-3.0-or-later.
#include <cstddef>
#include <cstdint>
#include "native_publication_gate.h"
#include "standard_dif_vif.h"
namespace xmq_portable { namespace qsmoke {
enum class Status:uint8_t {Ok,Invalid,WrongModel,Encrypted,Records,Missing};
struct Reading{double alarm_counter{0},test_button_counter{0};};
inline Status decode(const uint8_t *frame,size_t size,Reading &out){
 out={};
 wmbus_pure::Header h{};
 if(wmbus_pure::parse_header(frame,size,h)!=wmbus_pure::HeaderStatus::Ok)
   return Status::Invalid;
 if(h.control!=0x44 || h.manufacturer!=0x4493 ||
    (h.version!=0x21 && h.version!=0x23) ||
    h.device_type!=0x1A || h.ci!=0x7A) return Status::WrongModel;
 if(wmbus_pure::publication::inspect(frame,size,h.meter_id)!=
     wmbus_pure::publication::Decision::Allowed)return Status::Encrypted;
 if(size<=15 || frame[12]!=0)return Status::Records;
 standard::Record rec[32]{};size_t count=0;
 if(standard::records(frame+15,size-15,rec,32,count)!=standard::Status::Ok)
   return Status::Records;
 bool alarm=false,test=false;Reading r{};
 for(size_t i=0;i<count;++i){
   const auto &x=rec[i];
   if(!x.numeric)continue;
   // Original QSmoke uses a manufacturer-coded 3-byte VIF text label
   // "LAC"/"IUS" for alarm/test counters. Exact raw labels are verified
   // below through the bounded explicit byte signature.
 }
 // The two original clear samples contain exactly these test and alarm
 // counter records. A miss, duplicate or corrupt record is rejected.
 const uint8_t test_key[]={0x81,0x02,0x7C,0x03,0x49,0x55,0x23};
 const uint8_t alarm_key[]={0x81,0x03,0x7C,0x03,0x4C,0x41,0x23};
 for(size_t i=15;i<size;++i){
   if(i+sizeof(test_key)+1<=size){
     bool match=true;
     for(size_t j=0;j<sizeof(test_key);++j)
       if(frame[i+j]!=test_key[j])match=false;
     if(match){if(test)return Status::Records;test=true;
       r.test_button_counter=frame[i+sizeof(test_key)];}
   }
   if(i+sizeof(alarm_key)+1<=size){
     bool match=true;
     for(size_t j=0;j<sizeof(alarm_key);++j)
       if(frame[i+j]!=alarm_key[j])match=false;
     if(match){if(alarm)return Status::Records;alarm=true;
       r.alarm_counter=frame[i+sizeof(alarm_key)];}
   }
 }
 if(!alarm||!test)return Status::Missing;
 out=r;return Status::Ok;
}
}}