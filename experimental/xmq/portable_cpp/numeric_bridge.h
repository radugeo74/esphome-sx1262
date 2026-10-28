#pragma once
#include <cstddef>
#include <cstring>
#include "ixml_subset.h"
#include "record_decoder.h"

namespace xmq_portable {
enum class NumericStatus : uint8_t { Ok, MissingCapture, InvalidCapture, InvalidRecord };
struct NumericValue { double target_m3; double total_m3; };
inline NumericStatus decode_water_values(const Result &ixml, NumericValue &out) {
  bool prev_found = false, curr_found = false;
  double previous = 0, current = 0;
  for (size_t i = 0; i < ixml.count; ++i) {
    const Capture &capture = ixml.captures[i];
    const bool prev = std::strcmp(capture.name, "DV_prev") == 0;
    const bool curr = std::strcmp(capture.name, "DV_curr") == 0;
    if (!prev && !curr) continue;
    if (capture.length != 2 || (prev && prev_found) || (curr && curr_found))
      return NumericStatus::InvalidCapture;
    Record record{};
    const char *key = prev ? "4215" : "0215";
    if (decode_record(key, 4, capture.data, capture.length, record) != Status::Ok ||
        record.unit != Unit::VolumeM3)
      return NumericStatus::InvalidRecord;
    if (prev) { previous = record.scaled; prev_found = true; }
    else { current = record.scaled; curr_found = true; }
  }
  if (!prev_found || !curr_found) return NumericStatus::MissingCapture;
  out.target_m3 = previous;
  out.total_m3 = previous + current;
  return NumericStatus::Ok;
}
} // namespace xmq_portable
