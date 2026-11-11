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
    case Op::Unsigned32: {
      if(rule.bytes!=4) return false;
      const uint32_t raw=static_cast<uint32_t>(p[0]) |
          static_cast<uint32_t>(p[1])<<8 |
          static_cast<uint32_t>(p[2])<<16 |
          static_cast<uint32_t>(p[3])<<24;
      r.numeric[index]=static_cast<double>(raw)*rule.scale;
      return std::isfinite(r.numeric[index]);
    }
    case Op::Bcd4: {
      if(rule.bytes!=2) return false;
      for(unsigned i=0;i<2;++i)
        if((p[i]&0x0Fu)>9 || (p[i]>>4)>9) return false;
      const unsigned num=static_cast<unsigned>(p[1]>>4)*1000u+
         static_cast<unsigned>(p[1]&15u)*100u+
         static_cast<unsigned>(p[0]>>4)*10u+(p[0]&15u);
      r.numeric[index]=static_cast<double>(num)*rule.scale;
      return std::isfinite(r.numeric[index]);
    }
    case Op::Bcd8: {
      if(rule.bytes!=4) return false;
      uint64_t count=0;
      for(int i=3;i>=0;--i) {
        const unsigned hi=p[i]>>4,lo=p[i]&15u;
        if(hi>9 || lo>9)return false;
        count=count*100+hi*10+lo;
      }
      r.numeric[index]=static_cast<double>(count)*rule.scale;
      return std::isfinite(r.numeric[index]);
    }
    case Op::BcdVersion6: {
      if(rule.bytes!=3) return false;
      for(unsigned i=0;i<3;++i)
        if((p[i]&15u)>9 || (p[i]>>4)>9)return false;
      const int written=std::snprintf(
         r.formatted[index],sizeof(r.formatted[index]),
         "%02X%02X%02X",p[2],p[1],p[0]);
      return written==6;
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
  if(!f || n<17 ||
     !profile.rules || profile.rule_count==0 ||
     profile.rule_count>8 || !profile.models || !profile.states)
    return Status::Invalid;
  wmbus_pure::Header h{};
  if(wmbus_pure::parse_header(f,n,h)!=wmbus_pure::HeaderStatus::Ok)
    return Status::Invalid;
  if(h.control!=0x44 || h.ci!=0x7A)
    return Status::WrongModel;
  if(wmbus_pure::tpl_security::inspect(f,n).state!=
     wmbus_pure::tpl_security::State::NotEncrypted)
    return Status::EncryptedOrUnknown;
  // Security classification must precede source-fixture size admission:
  // an already-decrypted upstream iPERL fixture has N=1 and a different
  // length. It remains EncryptedOrUnknown, never a plaintext RF reading.
  if(n!=profile.verified_length) return Status::Invalid;
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
  const size_t first_record=15u+profile.leading_pad;
  if(profile.strict_layout) {
    if(profile.leading_pad>n-15 || profile.trailing_pad>n-15-
       profile.leading_pad) return Status::InvalidRecord;
    for(size_t k=15;k<first_record;++k)
      if(f[k]!=0x2F) return Status::InvalidRecord;
    offset=first_record;
  }
  while(offset<n) {
    if(profile.strict_layout && offset>=n-profile.trailing_pad) {
      if(seen!=((1u<<profile.rule_count)-1u)) return Status::Missing;
      for(size_t k=offset;k<n;++k)
        if(f[k]!=0x2F) return Status::InvalidRecord;
      offset=n;
      break;
    }
    if(f[offset]==0x2F) {
      if(profile.strict_layout || ending) return Status::InvalidRecord;
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
    if(profile.strict_layout && i!=static_cast<unsigned>(
       __builtin_popcount(static_cast<unsigned>(seen))))
      return Status::InvalidRecord;
    if(seen&(1u<<i)) return Status::Duplicate;
    const auto &rule=profile.rules[i];
    if(!apply(rule,f+offset+rule.key_len,candidate,i))
      return Status::InvalidRecord;
    seen|=1u<<i;
    offset+=rule.key_len+rule.bytes;
    if(profile.strict_layout && offset>n-profile.trailing_pad)
      return Status::InvalidRecord;
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

// XMQ source-compiled pulse profile: variable optional BCD12 records,
// but exact MVT/config/length/status/field mask from upstream fixtures.
// No live XMQ parsing, fallback mode or acceptance of unconfigured data.
inline Status decode_pulse(const uint8_t *f,size_t n,
                           const xmq_compiled_profiles::PulseProfile &profile,
                           Result &out,uint16_t &out_mask) {
  out={};out_mask=0;
  if(!f || n<19 || !profile.rules || profile.rule_count!=2 ||
     !profile.variants || !profile.states) return Status::Invalid;
  wmbus_pure::Header h{};
  if(wmbus_pure::parse_header(f,n,h)!=wmbus_pure::HeaderStatus::Ok)
    return Status::Invalid;
  if(h.control!=0x44 || h.ci!=0x7A)return Status::WrongModel;
  if(wmbus_pure::tpl_security::inspect(f,n).state!=
     wmbus_pure::tpl_security::State::NotEncrypted)
    return Status::EncryptedOrUnknown;
  const auto cfg=u16(f+13);
  const xmq_compiled_profiles::PulseVariant *variant=nullptr;
  for(size_t i=0;i<profile.variant_count;++i) {
    const auto &v=profile.variants[i];
    if(v.maker==h.manufacturer && v.ver==h.version &&
       v.medium==h.device_type && v.config==cfg &&
       v.length==n && v.tpl==f[12]) {
      if(variant) return Status::Invalid;
      variant=&v;
    }
  }
  if(!variant)return Status::WrongModel;
  if(f[15]!=0x2F || f[16]!=0x2F)return Status::InvalidRecord;
  Result decoded{};
  decoded.tpl_status=f[12];
  uint16_t seen=0;
  size_t at=17;
  while(at<n) {
    int found=-1;
    for(size_t i=0;i<profile.rule_count;++i) {
      const auto&r=profile.rules[i];
      if(n-at<static_cast<size_t>(r.key_len)+r.bytes)continue;
      bool same=true;
      for(size_t j=0;j<r.key_len;++j)
        if(f[at+j]!=r.key[j])same=false;
      if(same) {
        if(found>=0)return Status::InvalidRecord;
        found=static_cast<int>(i);
      }
    }
    if(found<0)return Status::InvalidRecord;
    const unsigned index=static_cast<unsigned>(found);
    if(seen&(1u<<index))return Status::Duplicate;
    if(index==0 && seen!=0)return Status::InvalidRecord;
    const auto&r=profile.rules[index];
    if(!apply(r,f+at+r.key_len,decoded,index))
      return Status::InvalidRecord;
    seen|=1u<<index;
    at+=r.key_len+r.bytes;
  }
  if(seen!=variant->fields_mask)return Status::Missing;
  for(size_t i=0;i<profile.state_count;++i)
    if(profile.states[i].tpl==f[12] && profile.states[i].error==0)
      decoded.status_text=profile.states[i].text;
  if(!decoded.status_text)return Status::UnsupportedStatus;
  out=decoded;out_mask=seen;return Status::Ok;
}

// XMQ source-compiled sparse standard record profile. Other upstream
// records remain opaque, with verified source-backed auxiliary signatures
// and strict size, model, TPL and format keys. No newly assumed alarm flags.
inline Status decode_sparse(const uint8_t *f,size_t n,
                            const xmq_compiled_profiles::SparseProfile &profile,
                            Result &out) {
  out={};
  if(!f || n<19 || n!=profile.length ||
     !profile.rules || profile.rule_count!=3 ||
     !profile.models || !profile.offsets || !profile.guards ||
     !profile.status_ok)return Status::Invalid;
  wmbus_pure::Header h{};
  if(wmbus_pure::parse_header(f,n,h)!=wmbus_pure::HeaderStatus::Ok)
    return Status::Invalid;
  if(h.control!=0x44 || h.ci!=0x7A)return Status::WrongModel;
  if(wmbus_pure::tpl_security::inspect(f,n).state!=
      wmbus_pure::tpl_security::State::NotEncrypted)
    return Status::EncryptedOrUnknown;
  const uint16_t cfg=u16(f+13);
  bool matched=false;
  for(size_t i=0;i<profile.model_count;++i) {
    const auto &m=profile.models[i];
    if(m.maker==h.manufacturer && m.ver==h.version &&
       m.medium==h.device_type && m.config==cfg)matched=true;
  }
  if(!matched)return Status::WrongModel;
  if(f[12]!=0)return Status::UnsupportedStatus;
  for(size_t i=0;i<profile.guard_count;++i) {
    const auto &g=profile.guards[i];
    if(g.length>3 || static_cast<size_t>(g.at)+g.length>n)
      return Status::InvalidRecord;
    for(size_t j=0;j<g.length;++j)
      if(f[g.at+j]!=g.key[j]) return Status::InvalidRecord;
  }
  Result decoded{};
  for(size_t i=0;i<profile.rule_count;++i) {
    const auto&r=profile.rules[i];
    const size_t at=profile.offsets[i];
    if(at>=n || at+static_cast<size_t>(r.key_len)+r.bytes>n)
      return Status::InvalidRecord;
    for(size_t j=0;j<r.key_len;++j)
      if(f[at+j]!=r.key[j])return Status::InvalidRecord;
    if(!apply(r,f+at+r.key_len,decoded,i))
      return Status::InvalidRecord;
  }
  if(decoded.raw16[2]!=0)return Status::UnsupportedStatus;
  decoded.tpl_status=f[12];
  decoded.status_text=profile.status_ok;
  out=decoded;return Status::Ok;
}
}} // namespace xmq_portable::full_xmq
