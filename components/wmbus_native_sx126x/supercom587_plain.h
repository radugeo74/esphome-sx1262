#pragma once
// Sontex Supercom587 original clear Mode0 source-compiled XMQ application.
// Record offsets/signatures, BCD conversions, MVT and status from XMQ.
// Shared interpreter, no per-driver parsing or deprecated wmbusmeters runtime.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "xmq_full_profile_runtime.h"
namespace xmq_portable { namespace supercom587 {
enum class Status:uint8_t {Ok,Invalid,WrongModel,Encrypted,BadRecords};
struct Reading {
 double total_m3{0};
 char software_version[7]{};
};
inline Status decode(const uint8_t *frame,size_t length,Reading &out) {
 out={};
 full_xmq::Result decoded{};
 const auto s=full_xmq::decode_sparse(
      frame,length,xmq_compiled_profiles::supercom587::profile,decoded);
 if(s!=full_xmq::Status::Ok) {
   if(s==full_xmq::Status::Invalid)return Status::Invalid;
   if(s==full_xmq::Status::WrongModel)return Status::WrongModel;
   if(s==full_xmq::Status::EncryptedOrUnknown)return Status::Encrypted;
   return Status::BadRecords;
 }
 if(decoded.formatted[1][6]!='\0')return Status::BadRecords;
 Reading next{};
 next.total_m3=decoded.numeric[0];
 std::memcpy(next.software_version,decoded.formatted[1],7);
 out=next;return Status::Ok;
}
}} // namespace xmq_portable::supercom587
