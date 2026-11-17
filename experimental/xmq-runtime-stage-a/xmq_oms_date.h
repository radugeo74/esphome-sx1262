#pragma once
// Generic projection of OMS Date DV records using the original XMQ match.
// This module deliberately does not infer a date from meter identity.
#include "xmq_dv_subset.h"
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace xmq_runtime {

struct DateProjectionError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

inline std::map<std::string, std::string> project_oms_dates(
    const Driver &driver, const std::vector<DecodedRecord> &records) {
  if (records.size()>64 || driver.fields.size()>256)
    throw DateProjectionError("OMS date projection size limit");
  std::map<std::string, std::string> out;
  for (const auto &field : driver.fields) {
    if (!field.has_match || !field.supported_match ||
        field.match_entire_payload || !field.calculate.empty() ||
        (field.quantity!="Text" && field.quantity!="PointInTime"))
      continue;
    const DecodedRecord *selected=nullptr;
    for (const auto &r : records) {
      if (r.meta.vif_range!="Date" || r.iso_date.empty() ||
          !matches(field,r.meta)) continue;
      if (selected) throw DateProjectionError("ambiguous OMS date field");
      selected=&r;
    }
    if (selected && !out.emplace(field.name,selected->iso_date).second)
      throw DateProjectionError("duplicate OMS date field");
  }
  return out;
}

}  // namespace xmq_runtime
