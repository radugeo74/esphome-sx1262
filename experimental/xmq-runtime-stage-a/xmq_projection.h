#pragma once
// Experimental Stage A: apply original XMQ field matching to decoded DV
// records. Only numeric Volume and Flow projections are implemented.
// No radio, XMQ formula execution, IXML, date or AES handling.
#include "xmq_dv_subset.h"
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace xmq_runtime {

struct ProjectedValue {
  std::string name;
  double number = 0;
  std::string unit;
};

struct ProjectionError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

// The accepted XMQ options must be explicit. Never silently treat an
// unsupported transformation as an upstream-equivalent result.
inline bool numeric_rule_supported(const FieldRule &rule) {
  if (!rule.has_match || !rule.supported_match ||
      rule.match_entire_payload || !rule.calculate.empty() ||
      !rule.ixml.empty()) return false;
  return rule.quantity == "Volume" || rule.quantity == "Flow";
}

// This is deliberately a constrained bridge for clear DV records. It will
// refuse ambiguous duplicate matches instead of choosing the wrong value.
inline std::vector<ProjectedValue> project_numeric_fields(
    const Driver &driver, const std::vector<DecodedRecord> &records) {
  if (records.size() > 64 || driver.fields.size() > 256)
    throw ProjectionError("projection resource limit");
  std::vector<ProjectedValue> out;
  for (const auto &rule : driver.fields) {
    if (!numeric_rule_supported(rule)) continue;
    const DecodedRecord *hit = nullptr;
    for (const auto &record : records) {
      if (!matches(rule, record.meta)) continue;
      const bool compatible =
          (rule.quantity == "Volume" && record.unit == "m3") ||
          (rule.quantity == "Flow" && record.unit == "m3h");
      if (!compatible) continue;
      if (hit) throw ProjectionError("ambiguous DV match for field: " + rule.name);
      hit = &record;
    }
    if (!hit) continue;
    ProjectedValue value{rule.name + "_" + hit->unit, hit->value, hit->unit};
    for (const auto &previous : out)
      if (previous.name == value.name)
        throw ProjectionError("duplicate output field: " + value.name);
    out.push_back(std::move(value));
  }
  return out;
}

inline const ProjectedValue *find_projected(
    const std::vector<ProjectedValue> &values, const std::string &name) {
  for (const auto &v : values)
    if (v.name == name) return &v;
  return nullptr;
}

}  // namespace xmq_runtime
