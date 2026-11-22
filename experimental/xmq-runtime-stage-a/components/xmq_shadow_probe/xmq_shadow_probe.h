#pragma once
// Experimental build-only probe. It does NOT subscribe to wmbus_radio,
// obtain AES keys, access Telegram, or modify any meter/sensor.
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "xmq_compat_include.h"
#include "xmq_live_capture_hook.h"
#include "xmq_shadow_budget.h"
#include "xmq_ci_security_census.h"
#include "xmq_shadow_memory_gate.h"
#include <memory>

namespace esphome {
namespace wmbus_meter { class Meter; }
namespace xmq_shadow_probe {

class ShadowProbe : public Component {
 public:
  void set_enabled(bool enabled) { enabled_ = enabled; }
  void set_target_meter(wmbus_meter::Meter *meter) { target_meter_ = meter; }
  void setup() override;
  void dump_config() override;
  void observe(const void *source_meter, const ::Telegram &telegram,
               bool decoded, const char *driver,
               const LegacyReadings &old_values) noexcept;
  // Early allocation-free check before the cloned legacy meter builds
  // per-field comparison snapshots. Does not gate legacy decoding.
  bool memory_headroom_available() noexcept;
  bool wants_readings(const void *source_meter) noexcept {
    if (target_meter_ != nullptr && source_meter != target_meter_) return false;
    if (!enabled_ || !registry_) return false;
    // A bounded budget PER hour, not one permanent budget per boot.
    // Subtraction remains valid across the uint32_t millis() wraparound.
    const uint32_t now = millis();
    if (budget_.renew_if_elapsed(now)) {
      inspected_ = 0;
      captured_ = 0;
      rejected_ = 0;
      unsupported_ = 0;
      no_reference_ = 0;
      low_memory_skipped_ = 0;
      min_largest_free_block_ = UINT32_MAX;
      diagnostic_ = xmq_runtime::ShadowDiagnostic(kMaxObservations, 0.051);
      ci_census_ = xmq_runtime::CiSecurityCensus{};
      diagnostic_.set_enabled(true);
      ESP_LOGI("xmq_shadow_probe",
               "XMQ shadow hourly observation budget renewed (max %u, read-only)",
               static_cast<unsigned>(kMaxObservations));
    }
    if (!budget_.may_observe()) return false;
    if (!memory_headroom_available()) {
      ++low_memory_skipped_;
      return false;
    }
    return true;
  }

 private:
  bool enabled_{false};
  wmbus_meter::Meter *target_meter_{nullptr};
  ShadowObservationBudget budget_{};
  uint32_t inspected_{0};
  uint32_t captured_{0};
  uint32_t rejected_{0};
  uint32_t unsupported_{0};
  uint32_t no_reference_{0};
  uint32_t low_memory_skipped_{0};
  std::unique_ptr<xmq_runtime::DriverRegistry> registry_;
  xmq_runtime::ShadowDiagnostic diagnostic_{128, 0.051};
  xmq_runtime::CiSecurityCensus ci_census_{};
  uint32_t min_free_heap_{UINT32_MAX};
  uint32_t min_largest_free_block_{UINT32_MAX};
  static constexpr uint32_t kMaxObservations = kShadowWindowMaxObservations;
};

}  // namespace xmq_shadow_probe
}  // namespace esphome
