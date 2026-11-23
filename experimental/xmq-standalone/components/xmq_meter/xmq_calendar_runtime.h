#pragma once
// Stage A date projection: evaluate PointInTime calculate expressions
// from the original XMQ file with numeric variables from the shared
// calculation engine. Does not interpret the raw OMS date VIF or statuses.
#include "xmq_calendar.h"
#include "xmq_numeric_runtime.h"
#include <map>
#include <stdexcept>
#include <string>

namespace xmq_runtime {

inline std::map<std::string,std::string> evaluate_calendar_fields(
    const Driver &driver,const std::map<std::string,Number> &numeric) {
  std::map<std::string,std::string> dates;
  for (const auto &field : driver.fields) {
    if (field.quantity!="PointInTime" || field.calculate.empty()) continue;
    if (!field.has_match && field.match_entire_payload)
      throw CalendarError("unsupported date payload projection");
    std::string date;
    try {
      date=CalendarExpression(numeric).evaluate(field.calculate);
    } catch (const CalendarError &error) {
      // Not every telegram provides the proprietary date raw records:
      // missing dependencies mean the date must remain absent, not stale.
      if (std::string(error.what()).rfind("unresolved variable:",0)==0)
        continue;
      throw;
    }
    if (!dates.emplace(field.name+"_date",date).second)
      throw CalendarError("duplicate calculated date field");
  }
  return dates;
}

} // namespace xmq_runtime
