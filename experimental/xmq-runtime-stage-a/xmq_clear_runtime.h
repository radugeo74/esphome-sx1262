#pragma once
// Stage A generic clear-application adapter.
// Input must already have link/CRC checked and AES decrypted, if required.
// It does not parse RF/link framing and is not connected to ESPHome.
// Only projected numeric Volume/Flow and OMS Date fields are supported.
#include "xmq_oms_date.h"
#include "xmq_projection.h"
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <map>

namespace xmq_runtime {

struct ClearRuntimeError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

struct ClearResult {
  std::string driver;
  std::vector<ProjectedValue> numeric;
  std::map<std::string,std::string> dates;
};

inline ClearResult decode_standard_clear(const Driver &driver,
                                          const Mvt &mvt,
                                          const std::vector<uint8_t> &application) {
  // This helper is intentionally strict: callers must supply a matching
  // inner-meter identity. Radio converters use the inner MVT, not the outer.
  if (!detects(driver,mvt)) throw ClearRuntimeError("driver MVT mismatch");
  auto records=decode_dv_subset(application);
  ClearResult result;
  result.driver=driver.name;
  result.numeric=project_numeric_fields(driver,records);
  result.dates=project_oms_dates(driver,records);
  return result;
}

}  // namespace xmq_runtime
