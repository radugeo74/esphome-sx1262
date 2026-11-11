#pragma once
// Lansen Pulse source-compiled XMQ profile. Shared bounded C++ decoder
// handles BCD12, record keys, optional counters and source-known statuses.
// Caller must verify complete radio DLL CRC before publication.
#include <cstddef>
#include <cstdint>
#include "xmq_full_profile_runtime.h"
namespace xmq_portable { namespace lansenpu {
enum class Status : uint8_t {
  Ok,InvalidFrame,WrongModel,EncryptedOrUnknown,BadRecords,
  DuplicateRecord,InvalidBcd,UnknownStatus,MissingCounter
};
struct Reading {
 bool has_a{false},has_b{false};
 double a_counter{0},b_counter{0};
 uint8_t tpl_status{0};
};
inline bool power_low(uint8_t tpl) {return (tpl&0x04u)!=0;}
inline bool permanent_error(uint8_t tpl) {return (tpl&0x08u)!=0;}
inline bool sabotage_enclosure(uint8_t tpl) {
 return (tpl&xmq_compiled_profiles::lansenpu::profile.sabotage)!=0;
}
inline const char*status_text(uint8_t tpl) {
 const auto &p=xmq_compiled_profiles::lansenpu::profile;
 for(size_t i=0;i<p.state_count;++i)
   if(p.states[i].tpl==tpl&&p.states[i].error==0)
     return p.states[i].text;
 return nullptr;
}
inline Status decode(const uint8_t *f,size_t n,Reading &out) {
 out={};
 full_xmq::Result decoded{};
 uint16_t mask=0;
 const auto s=full_xmq::decode_pulse(
     f,n,xmq_compiled_profiles::lansenpu::profile,decoded,mask);
 if(s!=full_xmq::Status::Ok) {
   if(s==full_xmq::Status::Invalid)return Status::InvalidFrame;
   if(s==full_xmq::Status::WrongModel)return Status::WrongModel;
   if(s==full_xmq::Status::EncryptedOrUnknown)
     return Status::EncryptedOrUnknown;
   if(s==full_xmq::Status::Duplicate)return Status::DuplicateRecord;
   if(s==full_xmq::Status::UnsupportedStatus)return Status::UnknownStatus;
   if(s==full_xmq::Status::Missing)return Status::MissingCounter;
   return Status::BadRecords;
 }
 Reading next{};
 next.tpl_status=decoded.tpl_status;
 next.has_a=(mask&1u)!=0;
 next.has_b=(mask&2u)!=0;
 next.a_counter=decoded.numeric[0];
 next.b_counter=decoded.numeric[1];
 out=next;return Status::Ok;
}
}} // namespace xmq_portable::lansenpu
