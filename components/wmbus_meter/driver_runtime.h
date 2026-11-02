#pragma once
// Experimental, allocation-free bridge from an already extracted wM-Bus
// application payload into generated XMQ driver rules. This is NOT a full
// telegram parser or a replacement for the existing SX1262 packet receiver.
#include <cstddef>
#include <cstdint>
#include "calendar_subset.h"

namespace xmq_portable {
struct DriverTables {
  const Rule *rules;
  size_t rule_count;
  const CaptureKey *capture_keys;
  size_t capture_key_count;
  const FieldKey *field_keys;
  size_t field_key_count;
  const CalcFormula *calculations;
  size_t calculation_count;
  const CalendarFormula *date_calculations;
  size_t date_calculation_count;
};
struct DecodedPayload {
  CalcValue *values;
  size_t value_capacity;
  size_t value_count;
  CalendarDate *dates;
  size_t date_capacity;
  size_t date_count;
};
enum class DriverStatus : uint8_t {
  Ok, BadArguments, ParseError, RecordError, CalculateError, CalendarError
};
inline DriverStatus decode_driver_payload(const DriverTables &tables,
    const uint8_t *payload, size_t payload_length,
    Capture *captures, size_t capture_capacity,
    DecodedPayload &output) {
  output.value_count = 0;
  output.date_count = 0;
  if (!payload || !payload_length || !captures || !capture_capacity ||
      !output.values || !output.value_capacity ||
      !tables.rules || !tables.rule_count || !tables.capture_keys ||
      !tables.field_keys || !tables.field_key_count ||
      (tables.calculation_count && !tables.calculations) ||
      (tables.date_calculation_count && (!tables.date_calculations ||
                                        !output.dates ||
                                        tables.date_calculation_count > output.date_capacity)))
    return DriverStatus::BadArguments;

  Result parsed{captures, capture_capacity, 0, 0};
  if (execute(tables.rules, tables.rule_count, "decode",
              payload, payload_length, parsed) != ParseStatus::Ok)
    return DriverStatus::ParseError;

  size_t count = 0;
  if (seed_calculation_fields(parsed, tables.capture_keys, tables.capture_key_count,
                              tables.field_keys, tables.field_key_count,
                              output.values, count, output.value_capacity) != NumericStatus::Ok)
    return DriverStatus::RecordError;
  if (tables.calculation_count &&
      calculate(tables.calculations, tables.calculation_count, output.values,
                count, output.value_capacity) != CalcStatus::Ok)
    return DriverStatus::CalculateError;
  if (tables.date_calculation_count &&
      calculate_dates(tables.date_calculations, tables.date_calculation_count,
                      output.values, count, output.dates,
                      output.date_capacity) != CalendarStatus::Ok)
    return DriverStatus::CalendarError;

  output.value_count = count;
  output.date_count = tables.date_calculation_count;
  return DriverStatus::Ok;
}
} // namespace xmq_portable
