#pragma once
// Lansen Door/Window native compatibility adapter.
// Source-specific DIF/VIF records, BCD12 transforms, OPEN/CLOSED lookup,
// TPL statuses and errors are compiled from lansendw.xmq at build time.
// The only live implementation is a shared bounded C++ interpreter.
#include <cstddef>
#include <cstdint>
#include "xmq_full_profile_runtime.h"
namespace xmq_portable { namespace lansendw {
enum class Status : uint8_t {
 Ok, InvalidFrame, WrongModel, EncryptedOrUnknown, InvalidStatus,
 BadRecords, Duplicate, InvalidBcd, Missing
};
struct Reading {
 double a_counter{0};
 double b_counter{0};
 uint16_t digital_input{0};
 uint16_t error_flags{0};
 uint8_t tpl_status{0};
};
inline const char*state_text(uint16_t value) {
 const auto &p=xmq_compiled_profiles::lansendw::profile;
 return full_xmq::lookup(p,value);
}
inline const char*error_text(uint16_t error,uint8_t tpl) {
 const auto &p=xmq_compiled_profiles::lansendw::profile;
 for(size_t i=0;i<p.state_count;++i)
   if(p.states[i].tpl==tpl && p.states[i].error==error)
     return p.states[i].text;
 return nullptr;
}
inline bool confirmed_permanent_error(uint8_t tpl) {
 const auto &p=xmq_compiled_profiles::lansendw::profile;
 for(size_t i=0;i<p.state_count;++i)
   if(p.states[i].tpl==tpl) return (tpl&0x08u)!=0;
 return false;
}
inline uint8_t unknown_tpl_bits(uint8_t tpl) {
 return static_cast<uint8_t>(tpl&0x60u);
}
inline uint16_t unknown_error_bits(uint16_t error) {
 return error; // Unknown bits stay raw; XMQ maps no error bits.
}
inline Status decode(const uint8_t *frame,size_t length,Reading &out) {
 out={};
 full_xmq::Result decoded{};
 const auto result=full_xmq::decode(frame,length,
     xmq_compiled_profiles::lansendw::profile,decoded);
 if(result!=full_xmq::Status::Ok) {
   if(result==full_xmq::Status::WrongModel) return Status::WrongModel;
   if(result==full_xmq::Status::EncryptedOrUnknown)
     return Status::EncryptedOrUnknown;
   if(result==full_xmq::Status::UnsupportedStatus)
     return Status::InvalidStatus;
   if(result==full_xmq::Status::Duplicate) return Status::Duplicate;
   if(result==full_xmq::Status::Missing) return Status::Missing;
   return Status::BadRecords;
 }
 Reading next{};
 next.digital_input=decoded.raw16[0];
 next.error_flags=decoded.raw16[1];
 next.a_counter=decoded.numeric[2];
 next.b_counter=decoded.numeric[3];
 next.tpl_status=decoded.tpl_status;
 out=next;
 return Status::Ok;
}
}} // namespace xmq_portable::lansendw
