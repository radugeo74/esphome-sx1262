#pragma once
// Stage A: isolated interface for an ALREADY VERIFIED and DECRYPTED
// application payload. This does not parse WMBus frames, check CRC, decrypt
// keys or intercept live radio handlers. The caller must provide the INNER
// meter MVT, exact application bytes, and verified application encoding.
#include "xmq_calendar_runtime.h"
#include "xmq_driver_registry.h"
#include "xmq_oms_date.h"
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace xmq_runtime {

enum class ApplicationEncoding { StandardDv, OriginalIxml };

struct XmqFrameResult {
  std::string driver;
  std::map<std::string, Number> numeric;
  std::map<std::string, std::string> dates;
};

inline XmqFrameResult project_verified_application(
    const DriverRegistry &registry,
    const Mvt &inner_mvt,
    ApplicationEncoding encoding,
    const std::vector<uint8_t> &clear_application,
    const std::string &explicit_driver = "") {
  if (clear_application.empty() || clear_application.size()>1024)
    throw RuntimeError("invalid clear application size");
  const Driver &driver=registry.select(inner_mvt,explicit_driver);
  std::vector<DecodedRecord> records;
  if (encoding==ApplicationEncoding::StandardDv)
    records=decode_dv_subset(clear_application);
  else if (encoding==ApplicationEncoding::OriginalIxml)
    records=decode_ixml_numeric(driver,clear_application);
  else
    throw RuntimeError("unsupported application encoding");

  XmqFrameResult result;
  result.driver=driver.name;
  result.numeric=evaluate_numeric_fields(driver,records);
  if (encoding==ApplicationEncoding::OriginalIxml) {
    result.dates=evaluate_calendar_fields(driver,result.numeric);
  } else {
    const auto matched=project_oms_dates(driver,records);
    for (const auto &entry:matched) {
      // XMQ name=target, quantity=PointInTime publishes target_date;
      // a Text field named set_date stays set_date.
      std::string name=entry.first;
      for (const auto &field:driver.fields)
        if (field.name==entry.first && field.quantity=="PointInTime" &&
            field.calculate.empty()) {
          if (name.size()<5 || name.substr(name.size()-5)!="_date")
            name+="_date";
          break;
        }
      if (!result.dates.emplace(name,entry.second).second)
        throw RuntimeError("duplicate XMQ output date");
    }
  }
  return result;
}

} // namespace xmq_runtime
