#pragma once
// Lansen LAN-WMBUS-R4: strict unencrypted fixed-layout public profile.
// Profile 0B/32 only, independently configured ID and verified DLL CRC.
#include <cstddef>
#include <cstdint>
#include "pure_frame_header.h"
#include "tpl_security_probe.h"
namespace xmq_portable {namespace lansenrp{
enum class Status:uint8_t {Ok,InvalidFrame,WrongModel,Encrypted,InvalidStatus,BadRecords};
struct Reading{
 double total_routed_messages_counter{0},used_router_slots_counter{0};
 double seconds_to_mode_change_counter{0},listen_timer_value_counter{0};
 double pause_timer_value_counter{0},start_time_value_counter{0};
 double battery_v{0};
 bool listening{false},power_low{false};
};
inline uint16_t le16(const uint8_t *p){return uint16_t(p[0])|(uint16_t(p[1])<<8);}
inline uint32_t le32(const uint8_t *p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|
                                              (uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
inline Status decode(const uint8_t *f,size_t n,Reading &out){
  out={};wmbus_pure::Header h{};
  if(wmbus_pure::parse_header(f,n,h)!=wmbus_pure::HeaderStatus::Ok)return Status::InvalidFrame;
  if(h.control!=0x44 || h.manufacturer!=0x3033 || h.version!=0x0b ||
     h.device_type!=0x32 || h.ci!=0x7a)return Status::WrongModel;
  if(wmbus_pure::tpl_security::inspect(f,n).state!=
     wmbus_pure::tpl_security::State::NotEncrypted)return Status::Encrypted;
  if(n!=85 || (f[12]!=0 && f[12]!=4))return Status::InvalidStatus;
  // All original DIF/VIF+extension prefixes must be in their exact
  // positions. This deliberately rejects unfamiliar format variants.
  const struct Prefix{size_t pos;uint8_t size;uint8_t bytes[5];} p[]={
    {15,2,{0x2f,0x2f}},{17,3,{0x04,0xfd,0x3a}},
    {24,4,{0x82,0x40,0xfd,0x3a}},{30,3,{0x02,0xfd,0x0f}},
    {35,5,{0x81,0x80,0x40,0xfd,0x3a}},
    {41,5,{0x84,0xc0,0x40,0xfd,0x3a}},
    {50,3,{0x42,0xfd,0x3a}},{55,4,{0x82,0x01,0xfd,0x3a}},
    {61,4,{0xc1,0x01,0xfd,0x3a}},
    {67,4,{0x82,0x02,0xfd,0x3a}},
    {72,2,{0x06,0x6d}},{80,3,{0x02,0xfd,0x46}}};
  for(const auto &prefix:p)
    for(size_t i=0;i<prefix.size;++i)
      if(f[prefix.pos+i]!=prefix.bytes[i])return Status::BadRecords;
  if(f[40]>1 || f[65]>0x7f)return Status::BadRecords;
  Reading r{};
  r.total_routed_messages_counter=double(le32(f+20));
  r.used_router_slots_counter=double(le16(f+28));
  r.listening=f[40]==1;
  r.seconds_to_mode_change_counter=double(le32(f+46));
  r.listen_timer_value_counter=double(le16(f+53));
  r.pause_timer_value_counter=double(le16(f+59));
  r.start_time_value_counter=double(le16(f+70));
  r.battery_v=double(le16(f+83))/1000.0;
  r.power_low=f[12]==4;
  if(r.used_router_slots_counter>936 || r.battery_v<1.0 ||
     r.battery_v>6.0)return Status::BadRecords;
  out=r;return Status::Ok;
}
}}