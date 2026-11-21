#pragma once
// Experimental build-only probe. It does NOT subscribe to wmbus_radio,
// obtain AES keys, access Telegram, or modify any meter/sensor.
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "xmq_compat_include.h"
#include "xmq_live_capture_hook.h"
#include "xmq_shadow_budget.h"
#include <memory>

namespace esphome {
namespace xmq_shadow_probe {

class ShadowProbe : public Component {
 public:
  void set_enabled(bool enabled) { enabled_ = enabled; }
  void setup() override;
  void dump_config() override;
  void observe(const ::Telegram &telegram, bool decoded, const char *driver,
               const LegacyReadings &old_values) noexcept;
  bool wants_readings() noexcept {
    if (!enabled_ || !registry_) return false;
    // A bounded budget PER hour, not one permanent budget per boot.
    // Subtraction remains valid across the uint32_t millis() wraparound.
    const uint32_t now = millis();
    if (shadow_window_elapsed(now, window_started_ms_)) {
      window_started_ms_ = now;
      inspected_ = 0;
      captured_ = 0;
      rejected_ = 0;
      unsupported_ = 0;
      no_reference_ = 0;
      diagnostic_ = xmq_runtime::ShadowDiagnostic(kMaxObservations, 0.051);
      diagnostic_.set_enabled(true);
      ESP_LOGI("xmq_shadow_probe",
               "XMQ shadow hourly observation budget renewed (max %u, read-only)",
               static_cast<unsigned>(kMaxObservations));
    }
    return inspected_ < kMaxObservations;
  }

 private:
  bool enabled_{false};
  uint32_t window_started_ms_{0};
  uint32_t inspected_{0};
  uint32_t captured_{0};
  uint32_t rejected_{0};
  uint32_t unsupported_{0};
  uint32_t no_reference_{0};
  std::unique_ptr<xmq_runtime::DriverRegistry> registry_;
  xmq_runtime::ShadowDiagnostic diagnostic_{128, 0.051};
  uint32_t min_free_heap_{UINT32_MAX};
  static constexpr uint32_t kMaxObservations = kShadowWindowMaxObservations;
};

}  // namespace xmq_shadow_probe
}  // namespace esphome
