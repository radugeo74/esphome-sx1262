#pragma once
// Stage A: generic XMQ driver registry and safe MVT selection.
// No per-meter branches; explicit selection still validates the meter MVT.
// Does not implement link-layer identities, auto driver priority or RF.
#include "xmq_clear_runtime.h"
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace xmq_runtime {

struct DriverSelectionError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

class DriverRegistry {
 public:
  explicit DriverRegistry(std::vector<Driver> drivers)
      : drivers_(std::move(drivers)) {
    if (drivers_.empty() || drivers_.size()>128)
      throw DriverSelectionError("invalid driver registry size");
    std::set<std::string> names;
    for (const auto &d : drivers_) {
      if (d.name.empty() || d.detect.empty() ||
          !names.insert(d.name).second)
        throw DriverSelectionError("driver missing name/detection or duplicate name");
    }
  }

  const Driver &select(const Mvt &meter,
                       const std::string &explicit_driver = "") const {
    if (!explicit_driver.empty()) {
      for (const auto &d:drivers_)
        if (d.name==explicit_driver) {
          if (!detects(d,meter))
            throw DriverSelectionError("explicit driver MVT mismatch");
          return d;
        }
      throw DriverSelectionError("explicit driver is not loaded");
    }
    const Driver *selected=nullptr;
    for (const auto &d:drivers_) {
      if (!detects(d,meter)) continue;
      if (selected)
        throw DriverSelectionError("ambiguous auto driver MVT");
      selected=&d;
    }
    if (!selected) throw DriverSelectionError("no matching XMQ driver");
    return *selected;
  }

  ClearResult decode_standard(const Mvt &meter,
                              const std::vector<uint8_t> &application,
                              const std::string &explicit_driver="") const {
    return decode_standard_clear(select(meter,explicit_driver),meter,application);
  }

 private:
  std::vector<Driver> drivers_;
};

}  // namespace xmq_runtime
