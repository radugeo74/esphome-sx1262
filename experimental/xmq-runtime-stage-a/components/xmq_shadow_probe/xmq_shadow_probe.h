#pragma once
// Experimental build-only probe. It does NOT subscribe to wmbus_radio,
// obtain AES keys, access Telegram, or modify any meter/sensor.
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "xmq_compat_include.h"

namespace esphome {
namespace xmq_shadow_probe {

class ShadowProbe : public Component {
 public:
  void set_enabled(bool enabled) { enabled_ = enabled; }
  void setup() override;
  void dump_config() override;

 private:
  bool enabled_{false};
};

}  // namespace xmq_shadow_probe
}  // namespace esphome
