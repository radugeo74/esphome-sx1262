#pragma once
// Experimental generic XMQ numeric path for a bounded subset.
// Flow: upstream XMQ -> IXML synthetic DIF/VIF -> standard DV decode ->
// XMQ match -> source-defined calculate expressions.
// Explicitly NOT compatible with all XMQ fields, dates, status or AES.
#include "xmq_dv_subset.h"
#include "xmq_expression.h"
#include "xmq_ixml_subset.h"
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace xmq_runtime {

struct RuntimeError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

// Build real DIF/VIF payload entries from the upstream IXML mapping;
// conversion and matching thereafter share the standard DV engine.
inline std::vector<DecodedRecord> decode_ixml_numeric(
    const Driver &driver, const std::vector<uint8_t> &clear_payload) {
  std::vector<DecodedRecord> out;
  const auto synthetic=decode_ixml_from_driver(driver,clear_payload);
  for (const auto &s:synthetic) {
    std::vector<uint8_t> encoded=parse_hex(s.difvifkey);
    if (encoded.size()+s.data.size()>1024)
      throw RuntimeError("synthetic DV size limit");
    encoded.insert(encoded.end(),s.data.begin(),s.data.end());
    auto decoded=decode_dv_subset(encoded);
    for (auto &item:decoded) out.push_back(std::move(item));
    if (out.size()>64) throw RuntimeError("DV output limit");
  }
  return out;
}

inline std::map<std::string, Number> evaluate_numeric_fields(
    const Driver &driver, const std::vector<DecodedRecord> &records) {
  if (records.size()>64 || driver.fields.size()>256)
    throw RuntimeError("XMQ numeric evaluation resource limit");
  std::map<std::string,Number> values;
  for (const auto &field:driver.fields) {
    if (field.has_match && field.supported_match &&
        field.calculate.empty() && !field.match_entire_payload) {
      const DecodedRecord *found=nullptr;
      for (const auto &entry:records) {
        if (!matches(field,entry.meta)) continue;
        if (found) throw RuntimeError("ambiguous XMQ match: "+field.name);
        found=&entry;
      }
      if (!found) continue;
      Dimension dimension=Dimension::Counter;
      std::string suffix="_counter";
      if (field.quantity=="Volume") {
        if (found->unit!="m3") throw RuntimeError("XMQ Volume unit mismatch");
        dimension=Dimension::VolumeM3;
        suffix="_m3";
      } else if (field.quantity=="HCA") {
        if (found->unit!="hca") throw RuntimeError("XMQ HCA unit mismatch");
        dimension=Dimension::HCA;
        suffix="_hca";
      } else if (field.quantity=="Temperature") {
        if (found->unit!="c") throw RuntimeError("XMQ Temperature unit mismatch");
        dimension=Dimension::TemperatureC;
        suffix="_c";
      } else if (field.quantity=="Dimensionless") {
        // A source-defined Dimensionless field may match a scaled physical
        // VIF (e.g. mkradio4 raw volume). Its numeric magnitude is
        // preserved while its logical XMQ unit is Counter.
        if (found->unit!="counter" && found->unit!="m3")
          throw RuntimeError("XMQ counter unit mismatch");
      } else {
        // Date, status, text and other unsupported fields not fabricated.
        continue;
      }
      if (!values.emplace(field.name+suffix, Number{found->value,dimension}).second)
        throw RuntimeError("duplicate XMQ raw field: "+field.name);
    }
  }
  // One generic pass follows XMQ field order. If upstream rules use
  // forward references, this fails instead of guessing or reordering.
  for (const auto &field:driver.fields) {
    if (field.calculate.empty()) continue;
    if (field.quantity!="Volume" && field.quantity!="Dimensionless")
      continue;
    Number result;
    try {
      result=NumericExpression(values).evaluate(field.calculate);
    } catch (const ExpressionError &error) {
      // A valid telegram may omit entire application records (e.g. OMS
      // 0x08 HCA without the proprietary billing-date fields). Never
      // synthesize a result from stale data or abort the entire telegram.
      // Other evaluation errors remain fatal, not silently ignored.
      if (std::string(error.what()).rfind("unresolved variable:",0)==0)
        continue;
      throw;
    }
    const Dimension expected=field.quantity=="Volume" ?
        Dimension::VolumeM3 : Dimension::Counter;
    if (result.dimension!=expected)
      throw RuntimeError("XMQ calculated unit mismatch: "+field.name);
    const std::string suffix=expected==Dimension::VolumeM3?"_m3":"_counter";
    if (!values.emplace(field.name+suffix,result).second)
      throw RuntimeError("duplicate calculated field: "+field.name);
  }
  return values;
}

} // namespace xmq_runtime
