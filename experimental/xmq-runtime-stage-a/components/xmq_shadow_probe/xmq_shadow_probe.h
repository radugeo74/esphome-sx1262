#pragma once
// Experimental build-only probe. It does NOT subscribe to wmbus_radio,
// obtain AES keys, access Telegram, or modify any meter/sensor.
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "xmq_compat_include.h"
#include "xmq_live_capture_hook.h"

namespace esphome {
namespace xmq_shadow_probe {

class ShadowProbe : public Component {
 public:
  void set_enabled(bool enabled) { enabled_ = enabled; }
  void setup() override;
  void dump_config() override;
  void observe(const ::Telegram &telegram, bool decoded) noexcept;

 private:
  bool enabled_{false};
  uint32_t inspected_{0};
  uint32_t captured_{0};
  uint32_t rejected_{0};
  uint32_t min_free_heap_{UINT32_MAX};
  static constexpr uint32_t kMaxObservations = 128;
};

}  // namespace xmq_shadow_probe
}  // namespace esphome
