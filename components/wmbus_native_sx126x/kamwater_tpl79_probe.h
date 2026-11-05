#pragma once
// Host-only TPL CI 79 compact data CRC verification. The upstream
// parse_TPL_79 reads signature and data CRC before expanding DIF/VIF;
// its paired golden fixtures show the data CRC covers reconstructed full
// DIF/VIF records, NOT the compressed payload itself.
// Requires independently DLL-verified, clear ELL application from caller.
// A valid CRC is error detection, never authentication or proof of ELL.
#include <cstddef>
#include <cstdint>
#include "compact_dif_vif.h"
#include "kamwater_compact_signature.h"
namespace xmq_portable { namespace kamwater { namespace tpl79 {
enum class Status : uint8_t {
  Ok, Invalid, UnsupportedCi, UnknownProfile, InvalidCompactData,
  DataCrcMismatch, Capacity
};
struct Reading {
  int profile_index{-1};
  size_t expanded_size{0};
};
inline Status verify(const uint8_t *tpl, size_t tpl_size,
                     uint8_t *expanded, size_t capacity,
                     Reading &result) {
  result={};
  if(!tpl || !expanded || tpl_size<6) return Status::Invalid;
  if(tpl[0]!=0x79) return Status::UnsupportedCi;
  const uint16_t format_signature=static_cast<uint16_t>(tpl[1]) |
                                   (static_cast<uint16_t>(tpl[2])<<8);
  int index=-1;
  if(signature::identify(format_signature,index)!=signature::Result::Matched)
    return Status::UnknownProfile;
  size_t count=0;
  const signature::Format *profiles=signature::formats(count);
  if(index<0 || static_cast<size_t>(index)>=count) return Status::UnknownProfile;
  size_t written=0;
  const compact::Status expansion=compact::expand(
      profiles[index].bytes,profiles[index].size,
      tpl+5,tpl_size-5,expanded,capacity,written);
  if(expansion==compact::Status::Capacity) return Status::Capacity;
  if(expansion!=compact::Status::Ok) return Status::InvalidCompactData;
  const uint16_t expected=static_cast<uint16_t>(tpl[3]) |
                           (static_cast<uint16_t>(tpl[4])<<8);
  if(signature::crc16_en13757(expanded,written)!=expected)
    return Status::DataCrcMismatch;
  result.profile_index=index;
  result.expanded_size=written;
  return Status::Ok;
}
}}} // namespace xmq_portable::kamwater::tpl79
