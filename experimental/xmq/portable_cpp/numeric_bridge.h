#pragma once
#include <cstddef>
#include <cstring>
#include "ixml_subset.h"
#include "record_decoder.h"

namespace xmq_portable {
enum class NumericStatus : uint8_t { Ok, MissingCapture, InvalidCapture, InvalidRecord };
struct NumericValue { double target_m3; double total_m3; };
inline NumericStatus lookup_mapped_record(const Result &ixml,
                                         const CaptureKey *annotations, size_t annotation_count,
                                         const FieldKey *fields, size_t field_count,
                                         const char *wanted, Record &out) {
  if (!annotations || !fields || !wanted) return NumericStatus::MissingCapture;
  const char *key = nullptr;
  for (size_t i = 0; i < field_count; ++i)
    if (std::strcmp(fields[i].field, wanted) == 0) {
      key = fields[i].difvif;
      break;
    }
  if (!key) return NumericStatus::MissingCapture;
  for (size_t i = 0; i < ixml.count; ++i) {
    const Capture &cap = ixml.captures[i];
    for (size_t j = 0; j < annotation_count; ++j) {
      if (std::strcmp(cap.name, annotations[j].capture) != 0 ||
          std::strcmp(key, annotations[j].difvif) != 0) continue;
      if (cap.length != 2) return NumericStatus::InvalidCapture;
      const auto status = decode_record(key, std::strlen(key), cap.data, cap.length, out);
      return status == Status::Ok ? NumericStatus::Ok : NumericStatus::InvalidRecord;
    }
  }
  return NumericStatus::MissingCapture;
}
inline NumericStatus decode_mapped_water(const Result &ixml,
                                         const CaptureKey *annotations, size_t annotation_count,
                                         const FieldKey *fields, size_t field_count,
                                         NumericValue &out) {
  Record prev{}, curr{};
  const auto a = lookup_mapped_record(ixml, annotations, annotation_count,
                                     fields, field_count, "prev_raw", prev);
  if (a != NumericStatus::Ok) return a;
  const auto b = lookup_mapped_record(ixml, annotations, annotation_count,
                                     fields, field_count, "curr_raw", curr);
  if (b != NumericStatus::Ok) return b;
  if (prev.unit != Unit::VolumeM3 || curr.unit != Unit::VolumeM3)
    return NumericStatus::InvalidRecord;
  out.target_m3 = prev.scaled;
  out.total_m3 = prev.scaled + curr.scaled;
  return NumericStatus::Ok;
}
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
