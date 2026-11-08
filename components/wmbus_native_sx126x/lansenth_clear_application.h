#pragma once
// Strict parser for already-clear Lansen TH XMQ application records.
// Never pass raw Mode-5 RF here. GPL-3.0-or-later original: lansenth.xmq.
#include <cstddef>
#include <cstdint>
namespace xmq_portable { namespace lansenth_clear {
enum class Status:uint8_t {Ok,Invalid,Missing,Duplicate,Unsupported};
struct Reading {
 double temperature_c{0},temperature_1h_c{0},temperature_24h_c{0};
 double humidity_rh{0},humidity_1h_rh{0},humidity_24h_rh{0};
};
inline int16_t signed16(const uint8_t *p) {
 const unsigned value=unsigned(p[0])|(unsigned(p[1])<<8);
 return int16_t(value<32768?int(value):int(value)-65536);
}
inline Status decode(const uint8_t*data,size_t len,Reading&out) {
 out={};
 if(!data || len<2 || data[0]!=0x2F || data[1]!=0x2F)
   return Status::Invalid;
 Reading r{};unsigned seen=0;bool on_time=false;size_t i=2;
 while(i<len) {
   if(data[i]==0x2F){++i;continue;}
   if(i+4==len && data[i]==0x02 && data[i+1]==0x23 && !on_time) {
     on_time=true;i+=4;continue; // optional public on_time_h record
   }
   unsigned slot=6;size_t header=0,bytes=0;double divisor=1;
   if(i+2<=len && data[i]==0x02 && data[i+1]==0x65) {
     slot=0;header=2;bytes=2;divisor=100;
   } else if(i+2<=len && data[i]==0x42 && data[i+1]==0x65) {
     slot=1;header=2;bytes=2;divisor=100;
   } else if(i+3<=len && data[i]==0x82 && data[i+1]==0x01 && data[i+2]==0x65) {
     slot=2;header=3;bytes=2;divisor=100;
   } else if(i+3<=len && data[i]==0x02 && data[i+1]==0xFB && data[i+2]==0x1A) {
     slot=3;header=3;bytes=2;divisor=10;
   } else if(i+3<=len && data[i]==0x42 && data[i+1]==0xFB && data[i+2]==0x1A) {
     slot=4;header=3;bytes=2;divisor=10;
   } else if(i+4<=len && data[i]==0x82 && data[i+1]==0x01 &&
             data[i+2]==0xFB && data[i+3]==0x1A) {
     slot=5;header=4;bytes=2;divisor=10;
   } else if(i+3<=len && data[i]==0x01 && data[i+1]==0xFB && data[i+2]==0x1B) {
     slot=3;header=3;bytes=1;
   } else if(i+3<=len && data[i]==0x41 && data[i+1]==0xFB && data[i+2]==0x1B) {
     slot=4;header=3;bytes=1;
   } else if(i+4<=len && data[i]==0x81 && data[i+1]==0x01 &&
             data[i+2]==0xFB && data[i+3]==0x1B) {
     slot=5;header=4;bytes=1;
   }
   if(slot>5 || i+header+bytes>len) return Status::Unsupported;
   if(seen&(1u<<slot)) return Status::Duplicate;
   const double value=bytes==1?double(data[i+header]):
                       double(signed16(data+i+header))/divisor;
   if((slot<3 && (value< -100 || value>150)) ||
      (slot>=3 && (value<0 || value>100))) return Status::Unsupported;
   switch(slot) {
     case 0:r.temperature_c=value;break;
     case 1:r.temperature_1h_c=value;break;
     case 2:r.temperature_24h_c=value;break;
     case 3:r.humidity_rh=value;break;
     case 4:r.humidity_1h_rh=value;break;
     case 5:r.humidity_24h_rh=value;break;
   }
   seen|=1u<<slot;i+=header+bytes;
 }
 if(seen!=63) return Status::Missing;
 out=r;return Status::Ok;
}
}}