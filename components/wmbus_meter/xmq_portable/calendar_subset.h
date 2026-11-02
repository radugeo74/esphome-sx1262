#pragma once
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstring>
#include "calculate_subset.h"

namespace xmq_portable {
struct CalendarDate { int year; int month; int day; };
struct CalendarFormula {
  const char *field;
  int base_year, base_month, base_day;
  const char *year_field, *month_field, *day_field;
};
enum class CalendarStatus : uint8_t { Ok, MissingField, InvalidDate, CapacityExceeded };

inline bool leap_year(int y) {
  return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0);
}
inline int days_in_month(int y, int m) {
  if (m < 1 || m > 12) return 0;
  static constexpr int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  return m == 2 && leap_year(y) ? 29 : days[m - 1];
}
inline bool calendar_field(const CalcValue *values, size_t count, const char *name, int &out) {
  if (!name) return false;
  for (size_t i = 0; i < count; ++i) {
    if (values[i].name && std::strcmp(values[i].name, name) == 0) {
      const double n = values[i].value;
      if (!std::isfinite(n) || n < -10000 || n > 10000 || std::floor(n) != n) return false;
      out = static_cast<int>(n);
      return true;
    }
  }
  return false;
}
inline CalendarStatus calculate_dates(const CalendarFormula *formulas, size_t count,
    const CalcValue *values, size_t value_count, CalendarDate *dates, size_t capacity) {
  if ((!formulas && count) || !values || !dates || count > capacity)
    return CalendarStatus::CapacityExceeded;
  for (size_t i = 0; i < count; ++i) {
    const auto &f = formulas[i];
    int yy = 0, mm = 0, dd = 0;
    if (!calendar_field(values, value_count, f.year_field, yy) ||
        !calendar_field(values, value_count, f.month_field, mm) ||
        !calendar_field(values, value_count, f.day_field, dd))
      return CalendarStatus::MissingField;
    if (f.base_year < 1 || f.base_year > 9999 ||
        f.base_day < 1 || f.base_day > days_in_month(f.base_year, f.base_month))
      return CalendarStatus::InvalidDate;
    // The supported XMQ subset is base-date + ((year*12+month-1)*1month)
    // + ((day-1)*24h). Perform Gregorian calendar arithmetic, never seconds.
    const int months = yy * 12 + mm - 1;
    const int month_index = f.base_year * 12 + f.base_month - 1 + months;
    if (month_index < 12 || month_index >= 120000) return CalendarStatus::InvalidDate;
    const int y = month_index / 12;
    const int m = month_index % 12 + 1;
    int d = f.base_day;
    const int limit = days_in_month(y, m);
    if (d > limit) d = limit;
    // XMQ date arithmetic must respect month lengths and year rollover.
    const int offset = dd - 1;
    if (offset < -366 || offset > 366) return CalendarStatus::InvalidDate;
    d += offset;
    int result_year = y, result_month = m;
    while (d > days_in_month(result_year, result_month)) {
      d -= days_in_month(result_year, result_month);
      if (++result_month == 13) { result_month = 1; ++result_year; }
      if (result_year > 9999) return CalendarStatus::InvalidDate;
    }
    while (d < 1) {
      if (--result_month == 0) { result_month = 12; --result_year; }
      if (result_year < 1) return CalendarStatus::InvalidDate;
      d += days_in_month(result_year, result_month);
    }
    dates[i] = {result_year, result_month, d};
  }
  return CalendarStatus::Ok;
}
}  // namespace xmq_portable
