#pragma once
// Sontex Supercom587 official XMQ warm/cold Mode-0 original fixture profile.
// GPL-3.0-or-later upstream: wmbusmeters/drivers/src/supercom587.xmq.
// The generated MVT, DIF/VIF keys, and per-record offsets are source-checked.
// No RF decryption, unconfigured publication, guessed status, or stale values.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include "native_publication_gate.h"
#include "standard_dif_vif.h"
#include "xmq_generated_clear_profiles.h"

namespace xmq_portable { namespace supercom587 {
enum class Status : uint8_t {Ok,Invalid,WrongModel,Encrypted,BadRecords};
struct Reading {
  double total_m3{0};
  char software_version[7]{}; // 6 BCD digits, including leading zeros
};

inline bool bcd_bytes(const uint8_t *data, size_t count) {
  if (!data) return false;
  for (size_t i=0;i<count;++i)
    if ((data[i] & 0x0Fu)>9 || (data[i]>>4)>9) return false;
  return true;
}

inline Status decode(const uint8_t *frame,size_t length,Reading &out) {
  out={};
  wmbus_pure::Header h{};
  if(wmbus_pure::parse_header(frame,length,h)!=wmbus_pure::HeaderStatus::Ok)
    return Status::Invalid;
  if(h.control!=0x44 || h.ci!=0x7A ||
     !xmq_generated_clear::match_model(h.manufacturer,h.version,h.device_type,
         xmq_generated_clear::supercom587::models,2))
    return Status::WrongModel;
  if(wmbus_pure::publication::inspect(frame,length,h.meter_id)!=
     wmbus_pure::publication::Decision::Allowed)
    return Status::Encrypted;
  const auto &rules=xmq_generated_clear::supercom587::rules;
  const auto &offsets=xmq_generated_clear::supercom587::offsets;
  const size_t verified_frame_bytes=
      xmq_generated_clear::supercom587::verified_frame_bytes;
  // The two original 163-byte CI=7A profiles share all three record offsets.
  // Refuse unverified variants rather than declaring a random tail valid.
  if(length!=verified_frame_bytes || frame[12]!=0 || frame[13]!=0 ||
     frame[14]!=0 || frame[21]!=0x42 || frame[22]!=0x6C ||
     frame[25]!=0x4C || frame[26]!=0x13)
    return Status::BadRecords;
  for(size_t i=0;i<3;++i) {
    if(offsets[i]>=length ||
       !xmq_generated_clear::match_rule(frame+offsets[i],
                                         length-offsets[i],rules[i]))
      return Status::BadRecords;
  }
  standard::Record first[1]{};size_t count=0;
  if(standard::records(frame+offsets[0],
       rules[0].key_len+rules[0].value_len,first,1,count)!=
       standard::Status::Ok || count!=1 ||
       first[0].dif!=0x0C || !first[0].numeric ||
       first[0].storage || first[0].function || first[0].tariff ||
       first[0].subunit || first[0].vif!=0x13 ||
       first[0].value<0)
    return Status::BadRecords;
  double volume=0;
  if(!standard::to_canonical(0x13,first[0].value,volume) ||
     volume<0 || volume>100000000)
    return Status::BadRecords;

  const uint8_t *version=frame+offsets[1]+rules[1].key_len;
  const uint8_t *error=frame+offsets[2]+rules[2].key_len;
  // The upstream library.software_version BCD record stores the least
  // significant pair first: 02 00 01 -> "010002".
  if(!bcd_bytes(version,rules[1].value_len))
    return Status::BadRecords;
  // Both official XMQ samples contain no error flags. Never publish
  // a nonzero flag as OK until its meaning is independently verified.
  if(error[0]!=0 || error[1]!=0) return Status::BadRecords;

  Reading next{};
  next.total_m3=volume;
  const int n=std::snprintf(next.software_version,
       sizeof(next.software_version),"%02X%02X%02X",
       static_cast<unsigned>(version[2]),
       static_cast<unsigned>(version[1]),
       static_cast<unsigned>(version[0]));
  if(n!=6) return Status::BadRecords;
  out=next;
  return Status::Ok;
}
}} // namespace xmq_portable::supercom587
