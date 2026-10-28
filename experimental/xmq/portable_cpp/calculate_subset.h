#pragma once
#include <cstddef>
#include <cstring>
#include "numeric_bridge.h"

namespace xmq_portable {
enum class CalcOp : uint8_t { Field, Number, Add, Multiply };
struct CalcStep { CalcOp op; const char *name; double number; };
struct CalcFormula { const char *field; const CalcStep *steps; size_t count; };
struct CalcValue { const char *name; double value; };
enum class CalcStatus : uint8_t { Ok, MissingField, InvalidProgram, CapacityExceeded };

inline CalcStatus calculate(const CalcFormula *formulas, size_t formula_count,
                            CalcValue *values, size_t &value_count, size_t capacity) {
  if (!formulas || !values || value_count > capacity) return CalcStatus::InvalidProgram;
  for (size_t fi = 0; fi < formula_count; ++fi) {
    const auto &f = formulas[fi];
    double stack[24]{};
    size_t depth = 0;
    if (!f.field || !f.steps || f.count > 48) return CalcStatus::InvalidProgram;
    for (size_t i = 0; i < f.count; ++i) {
      const auto &step = f.steps[i];
      if (step.op == CalcOp::Field) {
        bool found = false;
        for (size_t j = 0; j < value_count; ++j)
          if (step.name && std::strcmp(step.name, values[j].name) == 0) {
            if (depth >= 24) return CalcStatus::CapacityExceeded;
            stack[depth++] = values[j].value;
            found = true;
            break;
          }
        if (!found) return CalcStatus::MissingField;
      } else if (step.op == CalcOp::Number) {
        if (depth >= 24) return CalcStatus::CapacityExceeded;
        stack[depth++] = step.number;
      } else if (step.op == CalcOp::Add || step.op == CalcOp::Multiply) {
        if (depth < 2) return CalcStatus::InvalidProgram;
        const double right = stack[--depth];
        const double left = stack[--depth];
        stack[depth++] = step.op == CalcOp::Add ? left + right : left * right;
      } else return CalcStatus::InvalidProgram;
    }
    if (depth != 1 || value_count >= capacity) return CalcStatus::InvalidProgram;
    values[value_count++] = {f.field, stack[0]};
  }
  return CalcStatus::Ok;
}

inline NumericStatus seed_calculation_fields(const Result &result,
    const CaptureKey *captures, size_t capture_count,
    const FieldKey *fields, size_t field_count,
    CalcValue *values, size_t &count, size_t capacity) {
  if (!values || capacity < field_count) return NumericStatus::InvalidRecord;
  count = 0;
  for (size_t i = 0; i < field_count; ++i) {
    Record record{};
    const auto status = lookup_mapped_record(result, captures, capture_count, fields,
                                             field_count, fields[i].field, record);
    if (status != NumericStatus::Ok) return status;
    // XMQ Dimensionless record fields are represented as _counter.
    if (record.unit != Unit::VolumeM3 && record.unit != Unit::RawCounter)
      return NumericStatus::InvalidRecord;
    values[count++] = {fields[i].field, record.scaled};
  }
  return NumericStatus::Ok;
}
} // namespace xmq_portable
