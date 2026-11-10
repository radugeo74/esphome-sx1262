#pragma once
// Single bounded C++ interpreter for source-compiled XMQ standard records.
// No per-driver DIF/VIF tables, field conversion code or status lookup here.
// Caller must have independently validated on-air DLL CRC and configured ID.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include "pure_frame_header.h"
#include "tpl_security_probe.h"
#include "xmq_generated_full_profiles.h"

namespace xmq_portable { namespace full_xmq {
enum class Status : uint8_t {
  Ok, Invalid, WrongModel, EncryptedOrUnknown,
  UnsupportedStatus, InvalidRecord, Duplicate, Missing
};
struct Result {
  double numeric[8]{};
  uint16_t raw16[8]{};
  char formatted[8][32]{};
  uint8_t tpl_status{0};
  const char *status_text{nullptr};
  const char *lookup_text{nullptr};
};
inline uint16_t u16(const uint8_t *p) {
  return static_cast<uint16_t>(p[0]) |
         static_cast<uint16_t>(p[1])<<8;
}
inline bool apply(const xmq_compiled_profiles::Rule &rule,
                  const uint8_t *p, Result &r, size_t index) {
  if(!p || index>=8) return false;
  using Op=xmq_compiled_profiles::Op;
  switch(rule.op) {
    case Op::Signed16: {
      if(rule.bytes!=2) return false;
      const uint16_t word=u16(p);
      const int32_t signed_value=(word&0x8000u) ?
        static_cast<int32_t>(word)-65536 : static_cast<int32_t>(word);
      r.numeric[index]=static_cast<double>(signed_value)*rule.scale;
      return std::isfinite(r.numeric[index]);
    }
    case Op::Unsigned16: {
      if(rule.bytes!=2) return false;
      const uint16_t raw=u16(p);
      r.raw16[index]=raw;
      r.numeric[index]=static_cast<double>(raw)*rule.scale;
      return std::isfinite(r.numeric[index]);
    }
    case Op::Bcd12: {
      if(rule.bytes!=6) return false;
      uint64_t number=0;
      for(int i=5;i>=0;--i) {
        const unsigned hi=p[i]>>4,lo=p[i]&0x0Fu;
        if(hi>9 || lo>9) return false;
        number=number*100u+hi*10u+lo;
      }
      r.numeric[index]=static_cast<double>(number);
      return true;
    }
    case Op::Version3: {
      if(rule.bytes!=5 ||
         p[1]!='.' || p[3]!='.' ||
         p[0]<'0' || p[0]>'9' ||
         p[2]<'0' || p[2]>'9' ||
         p[4]<'0' || p[4]>'9') return false;
      const int written=std::snprintf(
         r.formatted[index],sizeof(r.formatted[index]),
         "%c.%c.%c",p[4],p[2],p[0]);
      return written==5;
    }
  }
  return false;
}
inline const char*lookup(const xmq_compiled_profiles::Profile &profile,
                         uint16_t value) {
  for(size_t i=0;i<profile.lookup_count;++i)
    if(profile.lookup[i].raw==value) return profile.lookup[i].text;
  return nullptr;
}
inline Status decode(const uint8_t *f,size_t n,
                     const xmq_compiled_profiles::Profile &profile,
                     Result &out) {
  out={};
  if(!f || n<17 || n!=profile.verified_length ||
     !profile.rules || profile.rule_count==0 ||
     profile.rule_count>8 || !profile.models || !profile.states)
    return Status::Invalid;
  wmbus_pure::Header h{};
  if(wmbus_pure::parse_header(f,n,h)!=wmbus_pure::HeaderStatus::Ok)
    return Status::Invalid;
  if(h.control!=0x44 || h.ci!=0x7A)
    return Status::WrongModel;
  const uint16_t cfg=u16(f+13);
  bool matched_model=false;
  for(size_t i=0;i<profile.model_count;++i) {
    const auto &m=profile.models[i];
    if(m.maker==h.manufacturer && m.ver==h.version &&
       m.medium==h.device_type && m.config==cfg) {
      matched_model=true;
      break;
    }
  }
  if(!matched_model) return Status::WrongModel;
  if(wmbus_pure::tpl_security::inspect(f,n).state!=
     wmbus_pure::tpl_security::State::NotEncrypted)
    return Status::EncryptedOrUnknown;
  const uint8_t tpl=f[12];
  bool known_status=false;
  for(size_t i=0;i<profile.state_count;++i)
    if(profile.states[i].tpl==tpl) known_status=true;
  if(!known_status) return Status::UnsupportedStatus;
  Result candidate{};
  candidate.tpl_status=tpl;
  uint16_t seen=0;
  bool ending=false;
  size_t offset=15;
  while(offset<n) {
    if(f[offset]==0x2F) {
      if(ending) return Status::InvalidRecord;
      ++offset;
      continue;
    }
    if(f[offset]==0x0F && offset+1==n && profile.trailing_0f &&
       seen==((1u<<profile.rule_count)-1u)) {
      ending=true;
      ++offset;
      continue;
    }
    if(ending) return Status::InvalidRecord;
    int matched=-1;
    for(size_t i=0;i<profile.rule_count;++i) {
      const auto &rule=profile.rules[i];
      if(n-offset<static_cast<size_t>(rule.key_len)+rule.bytes)
        continue;
      bool equal=true;
      for(size_t j=0;j<rule.key_len;++j)
        if(f[offset+j]!=rule.key[j]) equal=false;
      if(equal) {
        if(matched>=0) return Status::InvalidRecord;
        matched=static_cast<int>(i);
      }
    }
    if(matched<0) return Status::InvalidRecord;
    const unsigned i=static_cast<unsigned>(matched);
    if(seen&(1u<<i)) return Status::Duplicate;
    const auto &rule=profile.rules[i];
    if(!apply(rule,f+offset+rule.key_len,candidate,i))
      return Status::InvalidRecord;
    seen|=1u<<i;
    offset+=rule.key_len+rule.bytes;
  }
  if(seen!=((1u<<profile.rule_count)-1u) ||
     (profile.trailing_0f && !ending))
    return Status::Missing;
  const uint16_t error=(profile.error_index<profile.rule_count)
      ? candidate.raw16[profile.error_index] : 0;
  for(size_t i=0;i<profile.state_count;++i) {
    if(profile.states[i].tpl==tpl && profile.states[i].error==error) {
      candidate.status_text=profile.states[i].text;
      break;
    }
  }
  if(!candidate.status_text) return Status::UnsupportedStatus;
  if(profile.lookup_index<profile.rule_count) {
    if(!profile.lookup_count) return Status::UnsupportedStatus;
    candidate.lookup_text=lookup(profile,
        candidate.raw16[profile.lookup_index]);
    if(!candidate.lookup_text) return Status::UnsupportedStatus;
  }
  out=candidate;
  return Status::Ok;
}
}} // namespace xmq_portable::full_xmq
