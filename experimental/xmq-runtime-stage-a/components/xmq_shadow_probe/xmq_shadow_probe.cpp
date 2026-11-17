#include "xmq_shadow_probe.h"

namespace esphome {
namespace xmq_shadow_probe {
static const char *const TAG = "xmq_shadow_probe";

void ShadowProbe::setup() {
  if (!enabled_) {
    ESP_LOGI(TAG, "XMQ probe disabled; existing meter decoder untouched");
    return;
  }

  // Lab self-test only: synthetic clear payload, never a live radio frame.
  // Instantiate the real generic engine and shadow comparator in the
  // FULL Heltec ESPHome image to prove that C++ symbols actually link.
  xmq_runtime::Driver stub;
  stub.name = "xmq_build_only";
  stub.detect.push_back({"TCH", 0x95, 0x37});
  xmq_runtime::DriverRegistry registry({stub});
  xmq_runtime::ShadowDiagnostic diagnostic(1);
  diagnostic.set_enabled(true);
  xmq_runtime::ShadowReference reference;
  const auto result = diagnostic.inspect(
      registry, {"TCH", 0x95, 0x37},
      xmq_runtime::ApplicationEncoding::StandardDv,
      {0x43, 0x15, 0xA1, 0x00, 0x00}, reference);
  ESP_LOGI(TAG, "Synthetic-only XMQ linkage check result=%u",
           static_cast<unsigned>(result));
}

void ShadowProbe::dump_config() {
  ESP_LOGCONFIG(TAG, "Experimental XMQ build probe: %s (NO live telemetry)",
                enabled_ ? "synthetic lab self-test" : "disabled");
}

}  // namespace xmq_shadow_probe
}  // namespace esphome
