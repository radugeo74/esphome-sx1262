#pragma once
// Elvaco Sense 100W: strict keyless short-TPL profile, original elvsense.xmq.
// Caller must verify full radio/DLL CRC. Ignore already-clear NOKEY Mode-5.
#include <cstddef>
#include <cstdint>
#include "pure_frame_header.h"
#include "tpl_security_probe.h"
namespace xmq_portable {namespace elvsense {
enum class Status : uint8_t {Ok,InvalidFrame,WrongModel,Encrypted,InvalidStatus,BadRecords,Duplicate,Missing};
struct Reading{double temperature_c{0},battery_v{0};};
inline Status decode(const uint8_t *f,size_t n,Reading &out){
  out={};
  wmbus_pure::Header h{};
  if(wmbus_pure::parse_header(f,n,h)!=wmbus_pure::HeaderStatus::Ok)return Status::InvalidFrame;
  if(h.control!=0x44 || h.manufacturer!=0x1596 || h.version<0x50 ||
     h.version>0x54 || h.device_type!=0x1B || h.ci!=0x7A)return Status::WrongModel;
  if(wmbus_pure::tpl_security::inspect(f,n).state!=
     wmbus_pure::tpl_security::State::NotEncrypted)return Status::Encrypted;
  if(n<24 || f[12]!=0x00)return Status::InvalidStatus;
  Reading r{};unsigned seen=0;size_t i=15;
  while(i<n){
    if(f[i]==0x2f || f[i]==0x0f){++i;continue;}
    if(i+4<=n && f[i]==0x02 && f[i+1]==0x65){
      if(seen&1u)return Status::Duplicate;
      unsigned bits=unsigned(f[i+2])|(unsigned(f[i+3])<<8);
      const int value=bits<32768u?int(bits):int(bits)-65536;
      r.temperature_c=double(value)/100.0;
      if(r.temperature_c< -100.0||r.temperature_c>150.0)return Status::BadRecords;
      seen|=1u;i+=4;
    }else if(i+5<=n && f[i]==0x02 && f[i+1]==0xfd && f[i+2]==0x46){
      if(seen&2u)return Status::Duplicate;
      unsigned mv=unsigned(f[i+3])|(unsigned(f[i+4])<<8);
      r.battery_v=double(mv)/1000.0;
      if(mv<1000 || mv>6000)return Status::BadRecords;
      seen|=2u;i+=5;
    }else return Status::BadRecords;
  }
  if(seen!=3u)return Status::Missing;
  out=r;return Status::Ok;
}
}}