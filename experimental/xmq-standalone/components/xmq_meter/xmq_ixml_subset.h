#pragma once
// Experimental Stage A: deliberately limited, data-driven IXML interpreter.
// Reads grammar rules and DV mappings FROM the original .xmq file.
// Supported: alternatives, rule references, hex literals, byte, word, byte*,
// -Extra hidden rules and @DV_* captures; unsupported syntax fails closed.
// Not a full IXML implementation and not connected to ESPHome.
#include "xmq_semantic.h"
#include <algorithm>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace xmq_runtime {

struct IxmlError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

struct IxmlRecord {
  std::string difvifkey;
  std::vector<uint8_t> data;
  uint64_t unsigned_raw = 0;
};

inline std::string ixml_trim(const std::string &s) {
  const auto a=s.find_first_not_of(" \r\n\t");
  if (a==std::string::npos) return "";
  const auto b=s.find_last_not_of(" \r\n\t");
  return s.substr(a,b-a+1);
}

// Delimit at punctuation OUTSIDE both quote types and bracket character
// classes. Grammar sources under test do not contain escaped delimiters.
inline std::vector<std::string> ixml_split(const std::string &s, char delimiter) {
  std::vector<std::string> result;
  char quote=0;
  unsigned brackets=0;
  size_t start=0;
  for (size_t i=0;i<s.size();i++) {
    const char c=s[i];
    if (quote) {
      if (c==quote) quote=0;
      else if (c=='\\' && i+1<s.size()) ++i;
      continue;
    }
    if (c=='\'' || c=='"') {quote=c;continue;}
    if (c=='[') {++brackets;continue;}
    if (c==']') {
      if (!brackets) throw IxmlError("unexpected bracket");
      --brackets;
      continue;
    }
    if (!brackets && c==delimiter) {
      result.push_back(ixml_trim(s.substr(start,i-start)));
      start=i+1;
    }
  }
  if (quote || brackets) throw IxmlError("unbalanced IXML source");
  result.push_back(ixml_trim(s.substr(start)));
  return result;
}

class IxmlSubset {
 public:
  explicit IxmlSubset(const std::string &source) {
    if (source.size()>16384) throw IxmlError("IXML size limit");
    for (const auto &statement:ixml_split(source,'.')) {
      if (statement.empty()) continue;
      const auto equal=statement.find('=');
      if (equal==std::string::npos) throw IxmlError("IXML statement lacks '='");
      const std::string lhs=ixml_trim(statement.substr(0,equal));
      const std::string rhs=ixml_trim(statement.substr(equal+1));
      if (lhs.empty() || rhs.empty() || rules_.size()>=128)
        throw IxmlError("invalid IXML rule");
      if (!rules_.emplace(lhs,rhs).second) throw IxmlError("duplicate IXML rule");
    }
    if (!rules_.count("decode")) throw IxmlError("no IXML decode entry");
    for (const auto &entry:rules_) {
      const auto at=entry.first.find(">dvk");
      if (at==std::string::npos) continue;
      if (at+4!=entry.first.size()) throw IxmlError("unsupported DV mapping");
      std::string rhs=ixml_trim(entry.second);
      if (rhs.size()<4 || rhs[0]!='+' || rhs[1]!='\'' ||
          rhs.back()!='\'') throw IxmlError("unsupported DV key expression");
      const auto name=entry.first.substr(0,at);
      const auto key=rhs.substr(2,rhs.size()-3);
      if (key.empty() || key.size()>16 || key.size()%2)
        throw IxmlError("invalid DV key length");
      for (char c:key)
        if (!std::isxdigit(static_cast<unsigned char>(c)))
          throw IxmlError("invalid DV key");
      mappings_[name]=upper(key);
    }
  }

  std::vector<IxmlRecord> decode(const std::vector<uint8_t> &payload) const {
    if (payload.size()>1024) throw IxmlError("payload size limit");
    for (const auto &candidate:ixml_split(rules_.at("decode"),'|')) {
      size_t pos=0, steps=0;
      std::vector<IxmlRecord> records;
      try {
        expand(candidate,payload,pos,records,0,steps);
        if (pos==payload.size() && records.size()<=32) return records;
      } catch (const IxmlError &) {
        // Independent alternatives are tried without returning partial data.
      }
    }
    throw IxmlError("no IXML grammar alternative matches complete payload");
  }

 private:
  std::map<std::string,std::string> rules_;
  std::map<std::string,std::string> mappings_;

  void expand(const std::string &symbol, const std::vector<uint8_t> &bytes,
              size_t &pos, std::vector<IxmlRecord> &records,
              unsigned depth, size_t &steps) const {
    if (++steps>4096 || depth>24) throw IxmlError("IXML complexity limit");
    const std::string name=ixml_trim(symbol);
    if (name.empty()) throw IxmlError("empty grammar atom");
    if (name=="byte*" ) { pos=bytes.size(); return; }
    if (name=="byte" || name=="word") {
      const size_t amount=name=="word"?2:1;
      if (pos>bytes.size() || amount>bytes.size()-pos)
        throw IxmlError("IXML input truncated");
      pos+=amount;
      return;
    }
    if (name[0]=='\'') {
      if (name.size()<3 || name.back()!='\'') throw IxmlError("invalid literal");
      const std::string hex=name.substr(1,name.size()-2);
      if (hex.size()%2 || hex.empty()) throw IxmlError("invalid hex literal");
      for (size_t i=0;i<hex.size();i+=2) {
        unsigned val=0;
        if (!parse_uint(hex.substr(i,2),16,val) ||
            pos>=bytes.size() || bytes[pos++]!=val)
          throw IxmlError("literal mismatch");
      }
      return;
    }
    if (name.rfind("@DV_",0)==0) throw IxmlError("orphan DV annotation");
    auto it=rules_.find(name);
    if (it==rules_.end()) it=rules_.find("-"+name);
    if (it==rules_.end()) throw IxmlError("unsupported IXML grammar atom: "+name);
    const size_t start=pos;
    std::string capture;
    for (const auto &part:ixml_split(it->second,',')) {
      if (part.rfind("@DV_",0)==0) {
        if (!capture.empty()) throw IxmlError("duplicate DV capture");
        capture=part.substr(1);
      } else {
        expand(part,bytes,pos,records,depth+1,steps);
      }
    }
    if (capture.empty()) return;
    const auto mapping=mappings_.find(capture);
    if (mapping==mappings_.end()) throw IxmlError("unknown DV mapping");
    if (pos-start==0 || pos-start>8 || records.size()>=32)
      throw IxmlError("invalid DV capture size");
    IxmlRecord rec;
    rec.difvifkey=mapping->second;
    rec.data.assign(bytes.begin()+static_cast<std::ptrdiff_t>(start),
                    bytes.begin()+static_cast<std::ptrdiff_t>(pos));
    for (size_t i=0;i<rec.data.size();i++)
      rec.unsigned_raw|=static_cast<uint64_t>(rec.data[i])<<(i*8);
    records.push_back(std::move(rec));
  }
};

inline std::vector<IxmlRecord> decode_ixml_from_driver(
    const Driver &driver, const std::vector<uint8_t> &payload) {
  const FieldRule *rule=nullptr;
  for (const auto &field:driver.fields) {
    if (field.match_entire_payload && !field.ixml.empty()) {
      if (rule) throw IxmlError("multiple IXML payload handlers not supported");
      rule=&field;
    }
  }
  if (!rule) throw IxmlError("driver has no supported IXML rule");
  return IxmlSubset(rule->ixml).decode(payload);
}

} // namespace xmq_runtime
