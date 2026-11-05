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
// OMS compact data CRC can reference a preceding FULL telegram (rather
// than a CRC of the *current* expanded compact measurements). The caller
// must supply the previously validated clear full application of the
// SAME configured meter; this function does not establish provenance.
// Compare its DIF/VIF layout against the already identified format
// before accepting it as a valid reference. Current compact payload
// integrity must be validated separately by DLL/ELL or authenticated
// transport. No publication is authorized by this helper alone.
inline Status verify_with_full_reference(
    const uint8_t *tpl,size_t tpl_size,
    const uint8_t *reference_full,size_t reference_size,
    uint8_t *expanded,size_t capacity,Reading &result) {
  result={};
  if(!tpl || !expanded || !reference_full || tpl_size<6 ||
     reference_size==0) return Status::Invalid;
  if(tpl[0]!=0x79) return Status::UnsupportedCi;
  const uint16_t format_signature=static_cast<uint16_t>(tpl[1]) |
      (static_cast<uint16_t>(tpl[2])<<8);
  int index=-1;
  if(signature::identify(format_signature,index)!=signature::Result::Matched)
    return Status::UnknownProfile;
  size_t count=0;
  const signature::Format *profiles=signature::formats(count);
  if(index<0 || static_cast<size_t>(index)>=count)
    return Status::UnknownProfile;
  // Match the reference full application's record layout exactly.
  // Neither a guessed profile nor an arbitrary unrelated CRC is enough.
  const auto &profile=profiles[index];
  size_t p=0,f=0;
  while(p<profile.size) {
    const size_t start=p;
    const uint8_t first=profile.bytes[p++];
    uint8_t dif=first;
    while(dif&0x80u) {
      if(p>=profile.size) return Status::InvalidCompactData;
      dif=profile.bytes[p++];
    }
    if(p>=profile.size) return Status::InvalidCompactData;
    uint8_t vif=profile.bytes[p++];
    while(vif&0x80u) {
      if(p>=profile.size) return Status::InvalidCompactData;
      vif=profile.bytes[p++];
    }
    const size_t header_size=p-start;
    const size_t data_size=standard::format_length(first&0x0Fu);
    if(data_size==SIZE_MAX || f>reference_size ||
       header_size>reference_size-f ||
       data_size>reference_size-f-header_size)
      return Status::InvalidCompactData;
    for(size_t k=0;k<header_size;++k)
      if(reference_full[f+k]!=profile.bytes[start+k])
        return Status::InvalidCompactData;
    f+=header_size+data_size;
  }
  if(f!=reference_size) return Status::InvalidCompactData;
  const uint16_t data_crc=static_cast<uint16_t>(tpl[3]) |
      (static_cast<uint16_t>(tpl[4])<<8);
  if(signature::crc16_en13757(reference_full,reference_size)!=data_crc)
    return Status::DataCrcMismatch;
  size_t written=0;
  const compact::Status expansion=compact::expand(
      profile.bytes,profile.size,tpl+5,tpl_size-5,
      expanded,capacity,written);
  if(expansion==compact::Status::Capacity) return Status::Capacity;
  if(expansion!=compact::Status::Ok) return Status::InvalidCompactData;
  result.profile_index=index;
  result.expanded_size=written;
  return Status::Ok;
}
}}} // namespace xmq_portable::kamwater::tpl79
