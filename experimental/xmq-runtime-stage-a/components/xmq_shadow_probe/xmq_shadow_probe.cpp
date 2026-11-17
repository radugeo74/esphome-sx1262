#include "xmq_shadow_probe.h"
#include "esphome/components/wmbus_common/wmbus.h"
#include "xmq_telegram_capture.h"
#include "esp_heap_caps.h"

namespace esphome {
namespace xmq_shadow_probe {
static const char *const TAG = "xmq_shadow_probe";
static ShadowProbe *active_probe = nullptr;

void observe_legacy_telegram(const ::Telegram &telegram, bool decoded) noexcept {
  if (active_probe != nullptr) active_probe->observe(telegram, decoded);
}

void ShadowProbe::observe(const ::Telegram &telegram, bool decoded) noexcept {
  if (!enabled_ || inspected_ >= kMaxObservations) return;
  ++inspected_;
  const auto heap = static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_8BIT));
  if (heap < min_free_heap_) min_free_heap_ = heap;
  xmq_runtime::VerifiedApplication snapshot;
  bool captured = false;
  if (xmq_runtime::encoding_matches_ci(
          xmq_runtime::ApplicationEncoding::StandardDv, telegram.tpl_ci)) {
    captured = xmq_runtime::capture_verified_payload(
        telegram, decoded, xmq_runtime::ApplicationEncoding::StandardDv,
        snapshot);
  } else if (xmq_runtime::encoding_matches_ci(
                 xmq_runtime::ApplicationEncoding::OriginalIxml, telegram.tpl_ci)) {
    captured = xmq_runtime::capture_verified_payload(
        telegram, decoded, xmq_runtime::ApplicationEncoding::OriginalIxml,
        snapshot);
  }
  if (captured) ++captured_;
  else ++rejected_;
  if (inspected_ == 1 || inspected_ % 32 == 0 || inspected_ == kMaxObservations) {
    ESP_LOGI(TAG, "Shadow capture-only: observed=%u captured=%u rejected=%u min_free_heap=%u",
             inspected_, captured_, rejected_, min_free_heap_);
  }
}


void ShadowProbe::setup() {
  if (!enabled_) {
    ESP_LOGI(TAG, "XMQ probe disabled; existing meter decoder untouched");
    return;
  }
  active_probe = this;

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
  ESP_LOGCONFIG(TAG, "Experimental XMQ lab shadow: %s (capture-only, NO sensor changes)",
                enabled_ ? "synthetic self-test + capped capture" : "disabled");
}

}  // namespace xmq_shadow_probe
}  // namespace esphome
