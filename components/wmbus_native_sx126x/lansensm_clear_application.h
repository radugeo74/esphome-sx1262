#pragma once
// OFFLINE supplied-clear XMQ application ONLY; upstream header is AES Mode 5.
// NOT an alarm / HA sensor. GPL-3.0-or-later reference: lansensm.xmq
#include <cstddef>
#include <cstdint>
namespace xmq_portable {namespace lansensm_clear {
enum class Status:uint8_t {Ok,Invalid,Missing,Duplicate,Unsupported};
struct Reading {uint16_t flags{0};uint32_t message_counter{0},minutes_since_test{0};bool smoke{false};};
inline uint32_t le32(const uint8_t*p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
inline Status decode(const uint8_t*d,size_t n,Reading&out){
out={};if(!d||n<2||d[0]!=0x2f||d[1]!=0x2f)return Status::Invalid;
Reading r{};unsigned mask=0;size_t i=2;
while(i<n){if(d[i]==0x2f){++i;continue;}
unsigned slot=3;size_t hdr=0,bytes=0;
if(i+4<=n&&d[i]==0x02&&d[i+1]==0xfd&&d[i+2]==0x97&&d[i+3]==0x1d){slot=0;hdr=4;bytes=2;}
else if(i+3<=n&&d[i]==0x04&&d[i+1]==0xfd&&d[i+2]==0x08){slot=1;hdr=3;bytes=4;}
else if(i+3<=n&&d[i]==0x04&&d[i+1]==0xfd&&d[i+2]==0x3a){slot=2;hdr=3;bytes=4;}
if(slot>2||i+hdr+bytes>n)return Status::Unsupported;
if(mask&(1u<<slot))return Status::Duplicate;
const uint8_t*v=d+i+hdr;
if(slot==0){r.flags=uint16_t(v[0])|(uint16_t(v[1])<<8);
if(r.flags&~uint16_t(0x013e))return Status::Unsupported;
r.smoke=(r.flags&4u)!=0;}
else if(slot==1)r.message_counter=le32(v);else r.minutes_since_test=le32(v);
mask|=1u<<slot;i+=hdr+bytes;}
if(mask!=7) return Status::Missing;
out=r;
return Status::Ok;
}
}}