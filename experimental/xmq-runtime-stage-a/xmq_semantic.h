#pragma once
// Stage A semantic subset: metadata, MVT detection and strict DV matching.
// This is NOT a telegram decoder: caller supplies already decoded DV records.
// Every unsupported matching clause fails closed instead of being ignored.
#include "xmq_reader.h"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace xmq_runtime {

struct Mvt {
  std::string manufacturer;
  uint8_t version = 0;
  uint8_t type = 0;
};

struct DvRecord {
  std::string difvifkey;           // Upstream DIF/VIF key, canonical hex
  std::string measurement_type;    // Instantaneous / Minimum / Maximum / ...
  std::string vif_range;           // Volume, VolumeFlow, Date, ...
  unsigned storage_nr = 0;
  unsigned tariff_nr = 0;
  unsigned subunit_nr = 0;
};

struct FieldRule {
  std::string name;
  std::string quantity;
  std::string calculate;         // Preserved; NOT executed by this layer
  std::string ixml;              // Preserved; NOT executed by this layer
  bool match_entire_payload = false;
  bool has_match = false;
  bool supported_match = true;
  std::vector<std::pair<std::string, std::string>> conditions;
};

struct Driver {
  std::string name;
  std::string meter_type;
  std::string manufacturer;
  std::vector<Mvt> detect;
  std::vector<FieldRule> fields;
};

inline std::string compact(std::string value) {
  value.erase(std::remove_if(value.begin(), value.end(),
                             [](unsigned char c) { return c == ' ' || c == '\t' ||
                                                            c == '\n' || c == '\r'; }),
              value.end());
  return value;
}

inline std::string upper(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
  return s;
}

inline bool parse_uint(std::string s, unsigned base, unsigned &out) {
  s = compact(s);
  if (s.empty()) return false;
  unsigned long long value = 0;
  for (const char c : s) {
    int digit = c >= '0' && c <= '9' ? c-'0' :
                c >= 'a' && c <= 'f' ? c-'a'+10 :
                c >= 'A' && c <= 'F' ? c-'A'+10 : -1;
    if (digit < 0 || static_cast<unsigned>(digit) >= base) return false;
    value = value * base + static_cast<unsigned>(digit);
    if (value > std::numeric_limits<unsigned>::max()) return false;
  }
  out = static_cast<unsigned>(value);
  return true;
}

inline Mvt parse_mvt(const std::string &value) {
  const std::string v = compact(value);
  const auto a = v.find(','), b = a == std::string::npos ? a : v.find(',', a+1);
  if (a == std::string::npos || b == std::string::npos ||
      v.find(',', b+1) != std::string::npos) throw ParseError("invalid detect.mvt");
  Mvt result;
  result.manufacturer = upper(v.substr(0,a));
  unsigned ver = 0, typ = 0;
  if (result.manufacturer.size() != 3 ||
      !parse_uint(v.substr(a+1,b-a-1),16,ver) ||
      !parse_uint(v.substr(b+1),16,typ) || ver > 255 || typ > 255)
    throw ParseError("invalid detect.mvt value");
  result.version = static_cast<uint8_t>(ver);
  result.type = static_cast<uint8_t>(typ);
  return result;
}

inline Driver load_driver(const std::string &source) {
  const auto doc = Parser().parse(source);
  const Node *top = nullptr;
  for (const auto &node : doc) {
    if (node.key == "driver" && node.block) {
      if (top) throw ParseError("multiple driver blocks");
      top = &node;
    }
  }
  if (!top) throw ParseError("missing driver block");
  Driver out;
  if (const auto *p = top->first("name")) out.name = compact(p->value);
  if (const auto *p = top->first("meter_type")) out.meter_type = compact(p->value);
  if (const auto *p = top->first("manufacturer")) out.manufacturer = compact(p->value);
  if (out.name.empty()) throw ParseError("missing driver name");
  if (const auto *detect = top->first("detect")) {
    for (const auto &entry : detect->children) {
      if (entry.key == "mvt") out.detect.push_back(parse_mvt(entry.value));
    }
  }
  if (const auto *fields = top->first("fields")) {
    for (const auto &entry : fields->children) {
      if (entry.key != "field" || !entry.block) continue;
      FieldRule field;
      if (const auto *n = entry.first("name")) field.name = compact(n->value);
      if (const auto *n = entry.first("quantity")) field.quantity = compact(n->value);
      if (const auto *n = entry.first("calculate")) field.calculate = n->value;
      if (const auto *n = entry.first("ixml")) field.ixml = n->value;
      if (const auto *n = entry.first("match_entire_payload"))
        field.match_entire_payload = compact(n->value) == "true";
      if (const auto *match = entry.first("match")) {
        field.has_match = true;
        for (const auto &cond : match->children) {
          const std::string &k = cond.key;
          if (cond.block || (k != "difvifkey" && k != "measurement_type" &&
                            k != "vif_range" && k != "storage_nr" &&
                            k != "tariff_nr" && k != "subunit_nr")) {
            field.supported_match = false;
          } else {
            field.conditions.emplace_back(k, compact(cond.value));
          }
        }
      }
      if (!field.name.empty()) out.fields.push_back(std::move(field));
    }
  }
  return out;
}

inline bool detects(const Driver &driver, const Mvt &meter) {
  for (const auto &m : driver.detect)
    if (m.manufacturer == upper(meter.manufacturer) &&
        m.version == meter.version && m.type == meter.type) return true;
  return false;
}

// Important: upstream defaults for storage/tariff/subunit are 0. This
// partial matcher is intentionally conservative and does not implement
// qualifiers beyond the explicitly supported keys.
inline bool matches(const FieldRule &field, const DvRecord &record) {
  if (!field.has_match || !field.supported_match) return false;
  bool has_storage = false, has_tariff = false, has_subunit = false;
  for (const auto &condition : field.conditions) {
    const auto &k = condition.first, &v = condition.second;
    if (k == "difvifkey") {
      if (upper(v) != upper(compact(record.difvifkey))) return false;
    } else if (k == "measurement_type") {
      if (v != compact(record.measurement_type)) return false;
    } else if (k == "vif_range") {
      if (v != compact(record.vif_range)) return false;
    } else {
      unsigned n = 0;
      if (!parse_uint(v,10,n)) return false;
      if (k == "storage_nr") { has_storage = true; if (n != record.storage_nr) return false; }
      if (k == "tariff_nr") { has_tariff = true; if (n != record.tariff_nr) return false; }
      if (k == "subunit_nr") { has_subunit = true; if (n != record.subunit_nr) return false; }
    }
  }
  return (has_storage || record.storage_nr == 0) &&
         (has_tariff || record.tariff_nr == 0) &&
         (has_subunit || record.subunit_nr == 0);
}

}  // namespace xmq_runtime
